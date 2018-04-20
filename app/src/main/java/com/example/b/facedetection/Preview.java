package com.example.b.facedetection;

/**
 * Created by b on 2017-11-30.
 */


import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.ImageFormat;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.YuvImage;
import android.hardware.Camera;
import android.os.Environment;
import android.util.Log;
import android.view.View;


class Preview  extends View implements Camera.PreviewCallback {

    private static final int CAMERA_HEIGHT = 320;
    private static final int CAMERA_WIDTH = 320;

    private boolean cameraPreviewValid = false;
    private final Lock cameraPreviewLock = new ReentrantLock();
    private final Paint edgePaint = new Paint();

    private Bitmap frameBmp;    //captured frame
    private Bitmap frameBmpGray;    //frame to grayscale

    private byte[] copyByteData;    //frame to byte

    private Bitmap[] mustache;
    private Bitmap[] glass;
    private Bitmap[] cap;
    private Matrix matrix = new Matrix();

    class CheckedPos{
        int posX;
        int posY;
        int multiple;
        int angle;
        int axis; // 0 - 정면 , 1 - X  , 2- Y
    }
    private CheckedPos cp;  //프레임 내 얼굴 위치


    public Preview(Context context) {
        super(context);
       /* edgePaint.setColor(Color.RED);
        edgePaint.setStrokeWidth(10);*/

        mustache = new Bitmap[20];
        glass = new Bitmap[20];
        cap = new Bitmap[20];

        for(int i=0;i<20;i++) {
            mustache[i] = BitmapFactory.decodeResource(context.getResources(),R.drawable.mustache);
            mustache[i] = Bitmap.createScaledBitmap(mustache[i], (int) ( 4*mustache[i].getWidth()*(1-(0.025*i))), (int) ( 4*mustache[i].getHeight()*(1-(0.025*i))),true);

            glass[i] = BitmapFactory.decodeResource(context.getResources(),R.drawable.glass);
            glass[i] = Bitmap.createScaledBitmap(glass[i], (int) ( 8*glass[i].getWidth()*(1-(0.025*i))), (int) ( 8*glass[i].getHeight()*(1-(0.025*i))),true);

            cap[i] = BitmapFactory.decodeResource(context.getResources(),R.drawable.cap);
            cap[i] = Bitmap.createScaledBitmap(cap[i], (int) ( 8*cap[i].getWidth()*(1-(0.025*i))), (int) ( 8*cap[i].getHeight()*(1-(0.025*i))),true);
        }

        cp = new CheckedPos();
    }

    private native void findFace(byte[] source, int width, int height, CheckedPos cp);
    private native byte[] colorGrayArray(byte[] ba);
    @Override
    protected void onDraw(Canvas canvas) {

        if (cameraPreviewLock.tryLock()) {
            try {
                if (cameraPreviewValid) {


                    /*byte[] bb = colorGrayArray(copyByteData);
                    frameBmp = returnBitmap(bb,frameBmp);*/

                    //long start = System.currentTimeMillis();  //시간 계산
                    findFace(copyByteData, CAMERA_WIDTH, CAMERA_HEIGHT, cp );   //cp에 얼굴 위치 저장
                    //long estimated = System.currentTimeMillis() - start;
                    //Log.i("lib","running time: " + estimated);

                    int index = multipleSize(cp.multiple);

                    if(cp.axis == 1 || cp.axis == 2)    //옆 모습일 경우ㅡ
                        index++;

                    canvas.drawBitmap(frameBmp, 0,0,null);

                    //canvas.drawRect(cp.posX, cp.posY, (float)(cp.posX + 200 *0.001 *cp.multiple), (float)(cp.posY + 200 *0.001 *cp.multiple), edgePaint);


                    matrix.setRotate(5 * cp.angle , (int)( (cp.posX +( 200 *0.001 *cp.multiple *0.5) -(mustache[index].getWidth()*0.5) )),
                            (int) ( cp.posY +( 200 *0.001 *cp.multiple *0.5) -(mustache[index].getHeight()*0.5) ));
                    canvas.setMatrix(matrix);
                    if(cp.multiple != 0)
                        canvas.drawBitmap(mustache[index], (int)( (cp.posX +( 200 *0.001 *cp.multiple *0.5) -(mustache[index].getWidth()*0.5) )),
                                (int) ( cp.posY +( 200 *0.001 *cp.multiple *0.5) -(mustache[index].getHeight()*0.5) + 20) ,null);

/*                    matrix.setRotate(5 * cp.angle , (int)( (cp.posX +( 200 *0.001 *cp.multiple *0.5) -(glass[index].getWidth()*0.5) )),
                            (int) ( cp.posY +( 200 *0.001 *cp.multiple *0.5) -(glass[index].getHeight()*0.5) ));
                    canvas.setMatrix(matrix);
                    canvas.drawBitmap(glass[index], (int)( (cp.posX +( 200 *0.001 *cp.multiple *0.5) -(glass[index].getWidth()*0.5) )),
                            (int) ( cp.posY +( 200 *0.001 *cp.multiple *0.1) -(glass[index].getHeight()*0.5) + 20) ,null);

                    matrix.setRotate(5 * cp.angle , (int)( (cp.posX +( 200 *0.001 *cp.multiple *0.5) -(cap[index].getWidth()*0.5) )),
                            (int) ( cp.posY +( 200 *0.001 *cp.multiple) -(cap[index].getHeight()*0.5) ));
                    canvas.setMatrix(matrix);
                    canvas.drawBitmap(cap[index], (int)( (cp.posX +( 200 *0.001 *cp.multiple *0.5) -(cap[index].getWidth()*0.5) )),
                            (int) ( cp.posY +( 200 *0.001 *cp.multiple)  -(cap[index].getHeight() ) - 120 ) ,null);
*/
                    cameraPreviewValid = false;
                }
            }
            finally {
                cameraPreviewLock.unlock();
            }
        }
    }


    public void onPreviewFrame(byte[] data, Camera camera) {
        if (cameraPreviewLock.tryLock()) {
            try {
                if(!cameraPreviewValid) {
                    cameraPreviewValid = true;

                    frameBmp = decodeNV21(data,camera.getParameters());     //NV21 to ARGB
                    frameBmp = Bitmap.createBitmap(frameBmp, 0, 0, CAMERA_WIDTH, CAMERA_HEIGHT);
                    frameBmpGray = convertToGrayScale(frameBmp);

                    ByteBuffer dst = ByteBuffer.allocate(frameBmpGray.getByteCount());
                    frameBmpGray.copyPixelsToBuffer(dst);
                    copyByteData = dst.array();
                }
            }
            finally {
                cameraPreviewLock.unlock();
                postInvalidate();
            }
        }
    }

    public static Bitmap convertToGrayScale(Bitmap src){
        ColorMatrix bwMatrix = new ColorMatrix();
        bwMatrix.setSaturation(0);
        final ColorMatrixColorFilter colorFilter = new ColorMatrixColorFilter(bwMatrix);
        Bitmap rBitmap = src.copy(Bitmap.Config.ARGB_8888, true);

        Paint paint = new Paint();
        paint.setColorFilter(colorFilter);
        Canvas myCanvas = new Canvas(rBitmap);

        myCanvas.drawBitmap(rBitmap, 0, 0, paint);
        return rBitmap;
    }

    public Bitmap returnBitmap(byte[] ba,Bitmap bitmap){
        Bitmap.Config configBmp = Bitmap.Config.valueOf(bitmap.getConfig().name());

        Bitmap bitmap_tmp = Bitmap.createBitmap(CAMERA_WIDTH, CAMERA_HEIGHT, configBmp);
        ByteBuffer buffer = ByteBuffer.wrap(ba);
        bitmap_tmp.copyPixelsFromBuffer(buffer);
        return bitmap_tmp;
    }


    public static void saveBitmaptoJpeg(Bitmap bitmap,String folder, String name){
        String ex_storage =Environment.getExternalStorageDirectory().getAbsolutePath();

        String foler_name = "/"+folder+"/";
        String file_name = name+".jpg";
        String string_path = ex_storage+foler_name;

        File file_path;
        try{
            file_path = new File(string_path);
            if(!file_path.isDirectory()){
                file_path.mkdirs();
            }
            FileOutputStream out = new FileOutputStream(string_path+file_name);

            bitmap.compress(Bitmap.CompressFormat.JPEG, 100, out);
            out.close();

        }
        catch(FileNotFoundException exception){
            Log.e("FileNotFoundException", exception.getMessage());
        }
        catch(IOException exception){
            Log.e("IOException", exception.getMessage());
        }
    }



    public Bitmap decodeNV21(byte[] data, Camera.Parameters ps){
        Bitmap retimage;

        int w = ps.getPreviewSize().width;
        int h = ps.getPreviewSize().height;

        YuvImage yuv_image = new YuvImage(data, ps.getPreviewFormat(), w, h, null);

        Rect rect = new Rect(0, 0, w, h);
        ByteArrayOutputStream out_stream = new ByteArrayOutputStream();
        yuv_image.compressToJpeg(rect, 100, out_stream);

        retimage = BitmapFactory.decodeByteArray(out_stream.toByteArray(), 0, out_stream.size());
        return retimage;
    }



    int multipleSize(int multiple){
        int size = 0;

        if(multiple >= 1000)
            size = 0;
        else if(multiple >= 975)
            size = 1;
        else if(multiple >= 950)
            size = 2;
        else if(multiple >= 925)
            size = 3;
        else if(multiple >= 900)
            size = 4;
        else if(multiple >= 875)
            size = 5;
        else if(multiple >= 850)
            size = 6;
        else if(multiple >= 825)
            size = 7;
        else if(multiple >= 800)
            size = 8;
        else if(multiple >= 775)
            size = 9;
        else if(multiple >= 750)
            size = 10;
        else if(multiple >= 725)
            size = 11;
        else if(multiple >= 700)
            size = 12;
        else if(multiple >= 675)
            size = 13;
        else if(multiple >= 650)
            size = 14;
        else if(multiple >= 625)
            size = 15;
        else if(multiple >= 600)
            size = 16;
        else if(multiple >= 575)
            size = 17;
        else if(multiple >= 550)
            size = 18;
        else if(multiple >= 500)
            size = 19;

        return size;
    }


}