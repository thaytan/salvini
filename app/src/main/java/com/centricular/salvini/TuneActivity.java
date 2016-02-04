package com.centricular.salvini;

import android.content.Intent;
import android.os.Bundle;
import android.support.design.widget.FloatingActionButton;
import android.support.design.widget.Snackbar;
import android.support.v7.app.AppCompatActivity;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Toast;
import android.widget.TextView;
import org.freedesktop.gstreamer.GStreamer;
import java.util.HashMap;
import java.util.Map;

public class TuneActivity extends AppCompatActivity {
    private static native boolean classInit();
    private native void nativeInit();
    private native void nativeFinalize();
    private native void nativePlay();
    private native void nativePause();
    private long native_custom_data;

    static {
            System.loadLibrary("gstreamer_android");
            System.loadLibrary("android-salvini");
            classInit();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        try {
            GStreamer.init(this);
        } catch (Exception e) {
            Toast.makeText(this, e.getMessage(), Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        setContentView(R.layout.activity_tune);

        FloatingActionButton fab = (FloatingActionButton) findViewById(R.id.fab);
        fab.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Intent intent = new Intent(TuneActivity.this,
                        SettingsActivity.class);
                startActivity(intent);
                Snackbar.make(view, "Replace with your own action", Snackbar.LENGTH_LONG)
                        .setAction("Action", null).show();
            }
        });

        nativeInit();
    }

    protected void onDestroy() {
        nativeFinalize();
        super.onDestroy();
    }

    /* Called from native code */
    private void setMessage(final String message) {
        final TextView tv = (TextView) this.findViewById(R.id.contentTextView);
        runOnUiThread (new Runnable() {
            public void run() {
            tv.setText(message);
            }
        });
    }

    private void onNewNoteInfo(final HashMap<String,Object> noteInfo) {
        final TextView tv = (TextView) this.findViewById(R.id.contentTextView);
        final Object[] summary = (Object[]) noteInfo.get("summary");

        runOnUiThread (new Runnable() {
            public void run() {
                String res = "";
                for (Object o : summary) {
                  @SuppressWarnings("unchecked")
                    Map<String, Object> e = (HashMap<String,Object>) o;
                  res = res + e.toString() + "\n";
                }
                tv.setText (res);
            }
        });
    }

    /* Called from native code */
    private void onGStreamerInitialized () {
      nativePlay();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_tune, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            return true;
        }

        return super.onOptionsItemSelected(item);
    }
}
