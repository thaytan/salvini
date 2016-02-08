package com.centricular.salvini;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.text.TextPaint;
import android.util.AttributeSet;
import android.view.View;

import java.util.HashMap;
import java.util.Map;

/**
 * TODO: document your custom view class.
 */
public class RttaView extends View {
    private String mLabelString;
    private int mRedColor = Color.RED;
    private int mTextColour = Color.WHITE;
    private float mTextDimension = 16;

    private TextPaint mTextPaint;
    private float mTextWidth;
    private float mTextHeight;

    private HashMap<String,Object> mNoteInfo;

    public RttaView(Context context) {
        super(context);
        init(null, 0);
    }

    public RttaView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(attrs, 0);
    }

    public RttaView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        init(attrs, defStyle);
    }

    public void update(HashMap<String,Object> noteInfo)
    {
        mNoteInfo = noteInfo;
        invalidate();
    }

    private void init(AttributeSet attrs, int defStyle) {
        // Load attributes
        final TypedArray a = getContext().obtainStyledAttributes(
                attrs, R.styleable.RttaView, defStyle, 0);

        mNoteInfo = null;

        mLabelString = a.getString(
                R.styleable.RttaView_labelString);
        mTextColour = a.getColor(
                R.styleable.RttaView_textColour,
                mTextColour);

        // Use getDimensionPixelSize or getDimensionPixelOffset when dealing with
        // values that should fall on pixel boundaries.
        mTextDimension = a.getDimension(R.styleable.RttaView_textDimension,  mTextDimension);

        a.recycle();

        // Set up a default TextPaint object
        mTextPaint = new TextPaint();
        mTextPaint.setFlags(Paint.ANTI_ALIAS_FLAG);
        mTextPaint.setTextAlign(Paint.Align.LEFT);

        // Update TextPaint and text measurements from attributes
        invalidateTextPaintAndMeasurements();
    }

    private void invalidateTextPaintAndMeasurements() {
        mTextPaint.setTextSize(mTextDimension);
        mTextPaint.setColor(mTextColour);
        mTextWidth = mTextPaint.measureText(mLabelString);

        Paint.FontMetrics fontMetrics = mTextPaint.getFontMetrics();
        mTextHeight = fontMetrics.bottom - fontMetrics.top;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        // TODO: consider storing these as member variables to reduce
        // allocations per draw cycle.
        int paddingLeft = getPaddingLeft();
        int paddingTop = getPaddingTop();
        int paddingBottom = getPaddingBottom();
        int paddingRight = getPaddingRight();
        // int paddingBottom = getPaddingBottom();

        int contentWidth = getWidth() - paddingLeft - paddingRight;
        int contentHeight = getHeight() - paddingTop - paddingBottom;

        // FIXME: Settings:
        int cutoff = (int) 10; // total_samples * 2; // gdata->getSamplesThreshold();
        int yellow_limit = 20; // gdata->getYellowLimit();
        int red_limit = 30; // gdata->getRedLimit();

        float ypos = paddingTop + mTextHeight + 12;
        float xpos = paddingLeft + contentWidth/2;
        float bar_height = mTextHeight-10;
        float bar_length=0;
        float max_bar_length = (contentWidth/2)-5;

        canvas.drawText("Flat", paddingLeft + 5, paddingTop + mTextHeight, mTextPaint);
        canvas.drawText("Sharp", paddingLeft + contentWidth - 5 - mTextPaint.measureText("Sharp"),
                paddingTop + mTextHeight, mTextPaint);

        ypos += mTextHeight;

        if (mNoteInfo == null)
            return;

        final Object[] summary = (Object[]) mNoteInfo.get("summary");

        Paint curPaint = new Paint();
        curPaint.setColor(0xFF000000);

        for (Object o : summary) {
           @SuppressWarnings("unchecked")
           Map<String, Object> e = (HashMap<String,Object>) o;
           int num_samples = (int) e.get("samples");
           String note_name = (String) e.get("name");
           int cents = (int) e.get("cents");

            if (num_samples > cutoff) {
                int color;

                //
                //  Red and yellow limits
                //
                if (cents > red_limit || cents < -red_limit) {
                    color = 0xFFFF0000;
                } else if (cents > yellow_limit || cents < -yellow_limit) {
                    color = 0xFFFFFF00;
                } else {
                    color = 0xFF00FF00;
                };
                //
                //  Figure this bar length
                //
                //	bar_length = (int) (((float) cents) / 50.0) * (float) max_bar_length;
                curPaint.setStrokeWidth(0.0f);
                curPaint.setStyle(Paint.Style.FILL);
                curPaint.setColor(color);

                canvas.drawText(note_name, paddingLeft + 5, ypos + mTextHeight, mTextPaint);

                bar_length = (((float) cents * (float) max_bar_length) / 50.0f);
                if (bar_length < 0) {
                    canvas.drawRect(xpos+bar_length,ypos,xpos, ypos + bar_height, curPaint);
                    curPaint.setStyle(Paint.Style.STROKE);
                    curPaint.setColor(0xFF000000);
                    canvas.drawRect(xpos+bar_length,ypos,xpos, ypos + bar_height, curPaint);
                } else {
                    canvas.drawRect(xpos, ypos, xpos + bar_length, ypos + bar_height, curPaint);
                    curPaint.setStyle(Paint.Style.STROKE);
                    curPaint.setColor(0xFF000000);
                    canvas.drawRect(xpos, ypos, xpos + bar_length, ypos + bar_height, curPaint);
                };
                //
                // Down to next bar
                //
                ypos += mTextHeight + 1;
            }
        }
        //
        //  Base line
        //
        curPaint.setColor(0xFF000000);
        curPaint.setStyle(Paint.Style.STROKE);

        canvas.drawLine(xpos, mTextHeight + paddingTop,
                xpos, ypos, curPaint);

    }
}
