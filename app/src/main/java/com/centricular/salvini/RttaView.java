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

    private void init(AttributeSet attrs, int defStyle) {
        // Load attributes
        final TypedArray a = getContext().obtainStyledAttributes(
                attrs, R.styleable.RttaView, defStyle, 0);

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
        int paddingRight = getPaddingRight();
       // int paddingBottom = getPaddingBottom();

        int contentWidth = getWidth() - paddingLeft - paddingRight;
        //int contentHeight = getHeight() - paddingTop - paddingBottom;

        // Draw the text.
        canvas.drawText(mLabelString,
                paddingLeft + (contentWidth - mTextWidth) / 2,
                paddingTop + mTextHeight, mTextPaint);
    }
}
