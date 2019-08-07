#include <string.h>
#include <jni.h>
#include <glib.h>
#include <gst/gst.h>
#include <pthread.h>

GST_DEBUG_CATEGORY_STATIC (debug_category);
#define GST_CAT_DEFAULT debug_category

GST_PLUGIN_STATIC_DECLARE(salvini_rtta);

/*
 * These macros provide a way to store the native pointer to CustomData, which might be 32 or 64 bits, into
 * a jlong, which is always 64 bits, without warnings.
 */
#if GLIB_SIZEOF_VOID_P == 8
# define GET_CUSTOM_DATA(env, thiz, fieldID) (CustomData *)(*env)->GetLongField (env, thiz, fieldID)
# define SET_CUSTOM_DATA(env, thiz, fieldID, data) (*env)->SetLongField (env, thiz, fieldID, (jlong)data)
#else
# define GET_CUSTOM_DATA(env, thiz, fieldID) (CustomData *)(jint)(*env)->GetLongField (env, thiz, fieldID)
# define SET_CUSTOM_DATA(env, thiz, fieldID, data) (*env)->SetLongField (env, thiz, fieldID, (jlong)(jint)data)
#endif

typedef struct _CustomData
{
  jobject app;
  GMainContext *context;
  GMainLoop *main_loop;
  guint bus_watch_id;
  gint64 position;
  gint64 duration;
  gboolean initialized;
  GstElement *pipe, *rtta;
} CustomData;

static pthread_t gst_app_thread;
static pthread_key_t current_jni_env;
static JavaVM *java_vm;
static jfieldID custom_data_field_id;
static jmethodID set_message_method_id;
static jmethodID on_new_note_info_method_id;
static jmethodID on_gstreamer_initialized_method_id;

/*
 * Private methods
  */
static JNIEnv *
attach_current_thread (void)
{
  JNIEnv *env;
  JavaVMAttachArgs args;

  GST_DEBUG ("Attaching thread %p", g_thread_self ());
  args.version = JNI_VERSION_1_4;
  args.name = NULL;
  args.group = NULL;

  if ((*java_vm)->AttachCurrentThread (java_vm, &env, &args) < 0) {
    GST_ERROR ("Failed to attach current thread");
    return NULL;
  }

  return env;
}

static void
detach_current_thread (void *env)
{
  GST_DEBUG ("Detaching thread %p", g_thread_self ());
  (*java_vm)->DetachCurrentThread (java_vm);
}

static JNIEnv *
get_jni_env (void)
{
  JNIEnv *env;

  if ((env = pthread_getspecific (current_jni_env)) == NULL) {
    env = attach_current_thread ();
    pthread_setspecific (current_jni_env, env);
  }

  return env;
}

static void
set_ui_message (const gchar * message, CustomData * data)
{
  JNIEnv *env = get_jni_env ();
  GST_DEBUG ("Setting message to: %s", message);
  jstring jmessage = (*env)->NewStringUTF (env, message);
  (*env)->CallVoidMethod (env, data->app, set_message_method_id, jmessage);
  if ((*env)->ExceptionCheck (env)) {
    GST_ERROR ("Failed to call Java method");
    (*env)->ExceptionClear (env);
  }
  (*env)->DeleteLocalRef (env, jmessage);
}

static gboolean
refresh_ui (CustomData * data)
{
  return TRUE;
}

static void
error_cb (GstBus * bus, GstMessage * msg, CustomData * data)
{
  GError *err;
  gchar *debug_info;
  gchar *message_string;

  gst_message_parse_error (msg, &err, &debug_info);
  message_string =
      g_strdup_printf ("Error received from element %s: %s",
      GST_OBJECT_NAME (msg->src), err->message);
  g_clear_error (&err);
  g_free (debug_info);
  set_ui_message (message_string, data);
  g_free (message_string);
}

static jobject
_gst_structure_to_hash_map (JNIEnv *env, const GstStructure *s);

static jobject
_gst_value_to_java (JNIEnv *env, const GValue *v)
{
  switch (G_VALUE_TYPE (v)) {
    case G_TYPE_STRING:
      return (*env)->NewStringUTF (env, g_value_get_string (v));
    case G_TYPE_INT: {
      jclass cls = (*env)->FindClass(env, "java/lang/Integer");
      jmethodID methodID = (*env)->GetMethodID(env, cls, "<init>", "(I)V");
      jobject i=(*env)->NewObject(env, cls, methodID, g_value_get_int (v));
      (*env)->DeleteLocalRef (env, cls);
      return i;
    }
    case G_TYPE_UINT64: {
      jclass cls = (*env)->FindClass(env, "java/lang/Long");
      jmethodID methodID = (*env)->GetMethodID(env, cls, "<init>", "(J)V");
      jobject i=(*env)->NewObject(env, cls, methodID, g_value_get_uint64 (v));
      (*env)->DeleteLocalRef (env, cls);
      return i;
    }
    case G_TYPE_INT64: {
      jclass cls = (*env)->FindClass(env, "java/lang/Long");
      jmethodID methodID = (*env)->GetMethodID(env, cls, "<init>", "(J)V");
      jobject i=(*env)->NewObject(env, cls, methodID, g_value_get_int64 (v));
      (*env)->DeleteLocalRef (env, cls);
      return i;
    }
    default:
        if (G_VALUE_TYPE (v) == GST_TYPE_STRUCTURE) {
          return _gst_structure_to_hash_map (env, gst_value_get_structure (v));
        }
        else if (G_VALUE_TYPE (v) == GST_TYPE_LIST) {
          int i, n;
          jobjectArray ja;
          jclass jobjectClass;
          jobjectClass = (*env)->FindClass(env, "java/lang/Object");

          n = gst_value_list_get_size (v);
          ja = (*env)->NewObjectArray(env, n, jobjectClass, NULL);

          for (i = 0; i < n; i++) {
            jobject lv = _gst_value_to_java (env, gst_value_list_get_value (v,  i));
            (*env)->SetObjectArrayElement(env, ja, i, lv);
            (*env)->DeleteLocalRef (env, lv);
          }
          (*env)->DeleteLocalRef (env, jobjectClass);
          return ja;
        }
    break;
  }
  return NULL;
}

struct _MapCtx {
  JNIEnv *env;
  jobject hashMap;
  jmethodID put;
} map_ctx;

static gboolean
_gst_value_into_map (GQuark field_id, const GValue * value,
     gpointer user_data)
{
  struct _MapCtx *map_ctx = (struct _MapCtx *)(user_data);
  JNIEnv *env = map_ctx->env;
  jobject jv = _gst_value_to_java (env, value);
  jstring jkey = (*env)->NewStringUTF (env, g_quark_to_string (field_id));

  (*env)->CallObjectMethod(env, map_ctx->hashMap, map_ctx->put, jkey, jv);
  (*env)->DeleteLocalRef (env, jkey);
  (*env)->DeleteLocalRef (env, jv);

  return TRUE;
}

static jobject
_gst_structure_to_hash_map (JNIEnv *env, const GstStructure *s)
{
  jclass mapClass = (*env)->FindClass(env, "java/util/HashMap");

  if (mapClass == NULL) {
     GST_WARNING ("Failed to find Java HashMap class!");
     return NULL;
  }

  jsize map_len = gst_structure_n_fields(s);
  jmethodID init = (*env)->GetMethodID(env, mapClass, "<init>", "(I)V");
  jobject hashMap = (*env)->NewObject(env, mapClass, init, map_len);
  jmethodID put = (*env)->GetMethodID(env, mapClass, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

  (*env)->DeleteLocalRef(env, mapClass);

  if (hashMap == NULL) {
    GST_WARNING ("Failed to create HashMap!");
    return NULL;
  }

  struct _MapCtx map_ctx = { env, hashMap, put };

  gst_structure_foreach ((GstStructure *)(s), _gst_value_into_map, &map_ctx);

  return hashMap;
}

static void
post_rtta_to_java (CustomData *data, const GstStructure *s)
{
  JNIEnv *env = get_jni_env ();
  jobject hashMap;

  if (!gst_structure_n_fields (s)) {
    GST_DEBUG ("Empty RTTA message received!");
    return;
  }

  hashMap = _gst_structure_to_hash_map (env, s);
  if (!hashMap)
    return;

  (*env)->CallVoidMethod (env, data->app, on_new_note_info_method_id, hashMap);
  if ((*env)->ExceptionCheck (env)) {
    GST_ERROR ("Failed to call Java method to post new RTTA info");
    (*env)->ExceptionClear (env);
  }
  (*env)->DeleteLocalRef(env, hashMap);
}

static gboolean
handle_msg (GstBus * bus, GstMessage * msg, void *user_data)
{
  CustomData *data = (CustomData *)(user_data);

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:
      error_cb (bus, msg, data);
      break;
    case GST_MESSAGE_ELEMENT: {
      const GstStructure *structure;

      structure = gst_message_get_structure (msg);
      if (!structure || !gst_structure_has_name (structure, "rtta"))
        break;
      post_rtta_to_java (data, structure);
      break;
    }
    default:
      break;
  }
  return TRUE;
}

static void
check_initialization_complete (CustomData * data)
{
  JNIEnv *env = get_jni_env ();
  /* Check if all conditions are met to report GStreamer as initialized.
   * These conditions will change depending on the application */
  if (!data->initialized &&     data->main_loop) {
    GST_DEBUG
        ("Initialization complete, notifying application. main_loop:%p",
        data->main_loop);

    (*env)->CallVoidMethod (env, data->app, on_gstreamer_initialized_method_id);
    if ((*env)->ExceptionCheck (env)) {
      GST_ERROR ("Failed to call Java method");
      (*env)->ExceptionDescribe (env);
      (*env)->ExceptionClear (env);
    }
    data->initialized = TRUE;
  }
}

static void
destroy_pipeline (CustomData *data)
{
  if (!data->pipe)
    return;

  gst_element_set_state (data->pipe, GST_STATE_NULL);
  g_source_remove (data->bus_watch_id);
  gst_object_unref (data->rtta);
  data->rtta = NULL;
  gst_object_unref (data->pipe);
  data->pipe = NULL;
}

static void *
app_function (void *userdata)
{
  JavaVMAttachArgs args;
  CustomData *data = (CustomData *) userdata;
  GSource *timeout_source;

  GST_DEBUG ("Creating pipeline in CustomData at %p", data);

  /* create our own GLib Main Context, so we do not interfere with other libraries using GLib */
  data->context = g_main_context_new ();
  g_main_context_push_thread_default(data->context);

  /* Register a function that GLib will call 4 times per second */
  timeout_source = g_timeout_source_new (250);
  g_source_set_callback (timeout_source, (GSourceFunc) refresh_ui, data, NULL);
  g_source_attach (timeout_source, data->context);
  g_source_unref (timeout_source);

  /* Create a GLib Main Loop and set it to run */
  GST_DEBUG ("Entering main loop... (CustomData:%p)", data);
  data->main_loop = g_main_loop_new (data->context, FALSE);
  check_initialization_complete (data);
  g_main_loop_run (data->main_loop);
  GST_DEBUG ("Exited main loop");
  g_main_loop_unref (data->main_loop);
  data->main_loop = NULL;

  /* Free resources */
  destroy_pipeline (data);

  g_main_context_pop_thread_default(data->context);
  g_main_context_unref (data->context);

  return NULL;
}

/*
 * Java Bindings
 */
static void
gst_native_init (JNIEnv * env, jobject thiz)
{
  CustomData *data = g_new0 (CustomData, 1);

  GST_PLUGIN_STATIC_REGISTER(salvini_rtta);
  GST_DEBUG ("Registered RTTA plugin");

  data->duration = GST_CLOCK_TIME_NONE;
  SET_CUSTOM_DATA (env, thiz, custom_data_field_id, data);
  GST_DEBUG ("Created CustomData at %p", data);
  data->app = (*env)->NewGlobalRef (env, thiz);
  GST_DEBUG ("Created GlobalRef for app object at %p", data->app);
  pthread_create (&gst_app_thread, NULL, &app_function, data);
}

static void
gst_native_finalize (JNIEnv * env, jobject thiz)
{
  CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
  if (!data)
    return;

  GST_DEBUG ("Quitting main loop...");
  g_main_loop_quit (data->main_loop);
  GST_DEBUG ("Waiting for thread to finish...");
  pthread_join (gst_app_thread, NULL);
  GST_DEBUG ("Deleting GlobalRef for app object at %p", data->app);
  (*env)->DeleteGlobalRef (env, data->app);
  GST_DEBUG ("Freeing CustomData at %p", data);
  g_free (data);
  SET_CUSTOM_DATA (env, thiz, custom_data_field_id, NULL);
  GST_DEBUG ("Done finalizing");
}

static void
gst_native_play (JNIEnv * env, jobject thiz)
{
  CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);

  if (!data)
    return;

  GST_DEBUG ("Starting RTTA");
  if (!data->pipe) {
    GstBus *bus;

    data->pipe = gst_parse_launch ("openslessrc preset=voice-recognition ! audioconvert ! salvinirtta name=rtta ! fakesink", NULL);
    data->rtta = gst_bin_get_by_name (GST_BIN (data->pipe), "rtta");
    bus = gst_element_get_bus (data->pipe);
    data->bus_watch_id = gst_bus_add_watch (bus, handle_msg, data);
    gst_object_unref (bus);
  }

  gst_element_set_state (data->pipe, GST_STATE_PLAYING);
}

static void
gst_native_pause (JNIEnv * env, jobject thiz)
{
  CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);

  if (!data)
    return;

  GST_DEBUG ("Stopping RTTA");
  destroy_pipeline(data);
}

static void
gst_native_reset (JNIEnv * env, jobject thiz)
{
  CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);

  if (!data || !data->rtta)
    return;
  g_signal_emit_by_name (data->rtta, "reset", NULL);
}

static jboolean
gst_class_init (JNIEnv * env, jclass klass)
{
  custom_data_field_id =
      (*env)->GetFieldID (env, klass, "native_custom_data", "J");
  GST_DEBUG ("The FieldID for the native_custom_data field is %p",
      custom_data_field_id);
  set_message_method_id =
      (*env)->GetMethodID (env, klass, "setMessage", "(Ljava/lang/String;)V");
  GST_DEBUG ("The MethodID for the setMessage method is %p",
      set_message_method_id);
  on_new_note_info_method_id =
      (*env)->GetMethodID (env, klass, "onNewNoteInfo", "(Ljava/util/HashMap;)V");
  GST_DEBUG ("The MethodID for the onNewNoteInfo method is %p",
      on_new_note_info_method_id);

  on_gstreamer_initialized_method_id =
      (*env)->GetMethodID (env, klass, "onGStreamerInitialized", "()V");
  GST_DEBUG ("The MethodID for the onGStreamerInitialized method is %p",
      on_gstreamer_initialized_method_id);

  if (!custom_data_field_id || !set_message_method_id
      || !on_gstreamer_initialized_method_id || !on_new_note_info_method_id ) {
    GST_ERROR
        ("The calling class does not implement all necessary interface methods");
    return JNI_FALSE;
  }
  return JNI_TRUE;
}

static JNINativeMethod native_methods[] = {
  {"nativeInit", "()V", (void *) gst_native_init},
  {"nativeFinalize", "()V", (void *) gst_native_finalize},
  {"nativePlay", "()V", (void *) gst_native_play},
  {"nativePause", "()V", (void *) gst_native_pause},
  {"nativeReset", "()V", (void *) gst_native_reset},
  {"classInit", "()Z", (void *) gst_class_init},
};


int
JNI_OnLoad (JavaVM * vm, void *reserved)
{
  JNIEnv *env = NULL;

  GST_DEBUG_CATEGORY_INIT (debug_category, "salvini", 0,
      "Salvini RTTA");
  gst_debug_set_threshold_for_name ("salvini", 6);

  java_vm = vm;

  if ((*vm)->GetEnv (vm, (void **) &env, JNI_VERSION_1_4) != JNI_OK) {
    GST_ERROR ("Could not retrieve JNIEnv");
    return 0;
  }
  jclass klass = (*env)->FindClass (env,
      "com/centricular/salvini/TuneActivity");
  (*env)->RegisterNatives (env, klass, native_methods,
      G_N_ELEMENTS (native_methods));

  pthread_key_create (&current_jni_env, detach_current_thread);

  return JNI_VERSION_1_4;
}
