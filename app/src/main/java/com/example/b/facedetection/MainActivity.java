package com.example.b.facedetection;

import android.os.Bundle;
import android.view.SurfaceHolder;
import android.app.Activity;
import android.hardware.Camera;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.WindowManager;
import android.widget.FrameLayout;
import java.io.IOException;

import static android.hardware.Camera.CameraInfo.CAMERA_FACING_BACK;


public class MainActivity extends Activity implements SurfaceHolder.Callback {

        static {
            System.loadLibrary("main");
        }
        private native void setting();

        private static final int CAMERA_RESOULTION_HEIGHT = 320;
        private static final int CAMERA_RESOULTION_WIDTH = 480;

        private Camera camera;
        private Preview edgeView;
        private SurfaceView cameraView;
        private FrameLayout frameLayout;

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

            // Get the entire screen
            getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);

            edgeView = new Preview(this);

            setting();

            // Create the surface for the camera to draw its preview on
            cameraView = new SurfaceView(this);
            cameraView.getHolder().setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
            cameraView.getHolder().addCallback(this);

            // Setup the layout where the cameraView is completely obscured by the edgeView
            frameLayout = new FrameLayout(this);
            frameLayout.addView(cameraView);
            frameLayout.addView(edgeView);
            setContentView(frameLayout);

            // Prevent camera preview from showing up first
            edgeView.postInvalidate();

        }

        @Override
        protected void onDestroy() {
            super.onDestroy();
        }

        @Override
        protected void onPause() {
            super.onPause();
            stopCameraPreview();
        }

        private void stopCameraPreview() {
            if (camera != null) {
                camera.setPreviewCallback(null);
                camera.stopPreview();
                camera.release();
                camera = null;
            }
        }

        private void startCameraPreview() {
            camera = Camera.open();
            try {
                camera.setPreviewDisplay(cameraView.getHolder());
            } catch (IOException e) {
                e.printStackTrace();
            }
            camera.setPreviewCallback(edgeView);
        }

        public static int setCameraDisplayOrientation(Activity activity, int cameraId, android.hardware.Camera camera) {
            android.hardware.Camera.CameraInfo info = new android.hardware.Camera.CameraInfo();
            android.hardware.Camera.getCameraInfo(cameraId, info);
            int rotation = activity.getWindowManager().getDefaultDisplay().getRotation();
            int degrees = 0;
            switch (rotation) {
                case Surface.ROTATION_0: degrees = 0; break;
                case Surface.ROTATION_90: degrees = 90; break;
                case Surface.ROTATION_180: degrees = 180; break;
                case Surface.ROTATION_270: degrees = 270; break;
            }

            int result;
            if (info.facing == Camera.CameraInfo.CAMERA_FACING_FRONT) {
                result = (info.orientation + degrees) % 360;
                result = (360 - result) % 360;  // compensate the mirror
            } else {  // back-facing
                result = (info.orientation - degrees + 360) % 360;
            }

            return result;
        }

        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            // camera orientation
            camera.setDisplayOrientation(setCameraDisplayOrientation(this, CAMERA_FACING_BACK,camera));
            // get Camera parameters
            Camera.Parameters params = camera.getParameters();
            // picture image orientation
            params.setPreviewSize(CAMERA_RESOULTION_WIDTH,CAMERA_RESOULTION_HEIGHT);
            params.setRotation(setCameraDisplayOrientation(this, CAMERA_FACING_BACK, camera));
            camera.setParameters(params);
            camera.setDisplayOrientation(0);
            camera.startPreview();

        }

        public void surfaceCreated(SurfaceHolder holder) {
            startCameraPreview();
        }

        public void surfaceDestroyed(SurfaceHolder holder) {
            stopCameraPreview();
        }





}