#include <jni.h>
#include "com_example_b_facedetection_MainActivity.h"
#include "Point.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <fstream>
#include <iostream>

#include<android/log.h>

using namespace std;

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO   , "libnav", __VA_ARGS__)
#define EIGEN_FACE 1
#define AVERAGE_DATA 2
#define DEFAULT_WINDOW_POS 60
#define DEFAULT_WINDOW_MOVE_POS 30
#define MAX_SEARCH_SIZE 3
#define MAX_INDEX_STAGE_1 8
#define MAX_INDEX_STAGE_2 2

//찾아낸 얼굴 정보
struct RecognizedFace {
    Point leftTop;
    Point rightBottom;
    int stage1[MAX_INDEX_STAGE_1];
    int stage2[MAX_INDEX_STAGE_2];
    int multiple;
    int angle;
    int axis;
};

//안드로이드로 넘길 최종 결과값
struct CheckedResult{
    int pointX;
    int pointY;
    int pointMultiple;
    int pointAngle;
    int pointAxis;

};

//프레임 전체 탐색
int detectFace();
//argb to grayscale
//void rgbToGray();
//최적 얼굴 찾기
int pickBestFace(int index);
//최소 임계값 불러오기
void getMinStages();
//픽셀 각도 회전
int rotateAngle(int inputX, int inputY, Point center, int angle);
//haar filter
int checker3Block(Point leftTop, Point rightBottom, Point sLeftTop, Point sRightBottom,Point tLeftTop, Point tRightBottom, Point faceCenter, int angle);
int checker2Block(Point leftTop, Point rightBottom, Point sLeftTop, Point sRightBottom,Point faceCenter, int angle);
//모델값 로드
void loadFaceData(int flag);
//size 변환
int multipleSizeIndex(int mul);
//Haar 필터 사이즈로 나누기 위한 필터크기 반환
int normalizeStage2(int k);
int normalizeStage1(int k);

//현재 미사용
int** g_thresHold[3];
//로드된 데이터 저장
int** g_stage2[3];
int** g_stage1[3];

//한 프레임에서 찾은 얼굴들 저장
RecognizedFace capturedFace[1000];

int cameraHeight, cameraWidth ;
int* buf;

extern "C" {

    //얼굴 모델 로드
    JNIEXPORT void JNICALL Java_com_example_b_facedetection_MainActivity_setting(JNIEnv *env, jobject obj){

        loadFaceData(EIGEN_FACE);
        //loadFaceData(1);
        //getMinStages();
    }

    //현재 미사용,
    JNIEXPORT jbyteArray JNICALL Java_com_example_b_facedetection_Preview_colorGrayArray(JNIEnv *env, jobject obj, jbyteArray pixels){

        int len = env->GetArrayLength(pixels);
        buf = new int[len];
        env->GetByteArrayRegion (pixels, 0, len, reinterpret_cast<jbyte *>(buf));

        //rgbToGray();

        jbyteArray ret = env->NewByteArray(len);

        env->SetByteArrayRegion (ret, 0, len, reinterpret_cast<jbyte *>(buf));
        return ret;

    }



    JNIEXPORT void JNICALL Java_com_example_b_facedetection_Preview_findFace(JNIEnv* env, jobject thiz, jbyteArray source ,jint width, jint height, jobject cp ) {

        cameraHeight = height;
        cameraWidth = width;

        //frame to buf
        int len = env->GetArrayLength(source);
        buf = new int[len];
        env->GetByteArrayRegion (source, 0, len, reinterpret_cast<jbyte *>(buf));

        //모델에 가장 근접한 capturedFace 인덱스 반환
        int capturedIndex = detectFace();
        jbyte *sb = (env)->GetByteArrayElements(source, NULL);

        jclass clazz;
        clazz = env->GetObjectClass(cp);

        jfieldID fid;

        //위치 C++ -> Java 반환
        CheckedResult user;
        fid = env->GetFieldID(clazz, "posX", "I");
        user.pointX = env->GetIntField(cp, fid);
        env->SetIntField(cp,fid,capturedFace[capturedIndex].leftTop.x);

        fid = env->GetFieldID(clazz, "posY", "I");
        user.pointY = env->GetIntField(cp, fid);
        env->SetIntField(cp,fid,capturedFace[capturedIndex].leftTop.y);

        fid = env->GetFieldID(clazz, "multiple", "I");
        user.pointMultiple = env->GetIntField(cp, fid);
        env->SetIntField(cp,fid,capturedFace[capturedIndex].multiple);

        fid = env->GetFieldID(clazz, "angle", "I");
        user.pointAngle = env->GetIntField(cp, fid);
        env->SetIntField(cp,fid,capturedFace[capturedIndex].angle);

        fid = env->GetFieldID(clazz, "axis", "I");
        user.pointAxis = env->GetIntField(cp, fid);
        env->SetIntField(cp,fid,capturedFace[capturedIndex].axis);

        free(capturedFace);

        (env)->ReleaseByteArrayElements( source, sb, JNI_COMMIT);

        delete[] buf;
    }
}



  int detectFace()
  {
      int stage2[MAX_INDEX_STAGE_2] = {0, }; //eye
      int stage1[MAX_INDEX_STAGE_1] = {0, }; //nose,eye0,eye1,mouth

      int size = 0;//사이즈 총 현재 3개
      double multiple = 1; //비율
      int index = 0;//한 프레임에서 찾은 얼굴 개수
      int angle = 0;//각도

      int tempDiff= 0;

      double x_axis = 1, y_axis = 1;//얼굴 돌아간 경우, 회전축

      for (int axisChange = 0; axisChange < 3; axisChange++) {

          //얼굴 찾는 사각형 좌표
          Point leftTop(DEFAULT_WINDOW_POS, DEFAULT_WINDOW_POS);
          Point rightBottom(DEFAULT_WINDOW_POS + 200, DEFAULT_WINDOW_POS + 200);
          multiple = 1;
          angle = 0;
          size = 0;

          while (true)
          {
              //X방향 한줄 체크 완료
              if (leftTop.x  >= DEFAULT_WINDOW_POS + DEFAULT_WINDOW_MOVE_POS ) {
                  leftTop.x = DEFAULT_WINDOW_POS;
                  leftTop.y += 5;
                  rightBottom.x = (DEFAULT_WINDOW_POS + 200) * multiple;
                  rightBottom.y += 5;
              }
              //한 크기의 프레임 전부 체크 완료
              else if (leftTop.y >= DEFAULT_WINDOW_POS + DEFAULT_WINDOW_MOVE_POS ) {

                  multiple -= (0.025);
                  size++;


                  if(size == MAX_SEARCH_SIZE) //최소 탐색 크기
                      break;

                  leftTop.x = DEFAULT_WINDOW_POS;
                  leftTop.y = DEFAULT_WINDOW_POS;
                  rightBottom.x = (DEFAULT_WINDOW_POS+200)* multiple;
                  rightBottom.y = (DEFAULT_WINDOW_POS+200)* multiple;

              }

                  //탐색
              else {

                  Point faceCenter(Point(leftTop.x + 100 * multiple * x_axis, leftTop.y + 75 * multiple * y_axis)); //확인된 얼굴 정중앙

                  //for (int i = 0; i < 3; i++) {
                      //코
                     /* stage1[0] = checker3Block(
                              Point(leftTop.x + 80 * multiple * x_axis,
                                    leftTop.y + 50 * multiple * y_axis),
                              Point(leftTop.x + 90 * multiple * x_axis,
                                    leftTop.y + 75 * multiple * y_axis),
                              Point(leftTop.x + 90 * multiple * x_axis,
                                    leftTop.y + 50 * multiple * y_axis),
                              Point(leftTop.x + 110 * multiple * x_axis,
                                    leftTop.y + 75 * multiple * y_axis),
                              Point(leftTop.x + 110 * multiple * x_axis,
                                    leftTop.y + 50 * multiple * y_axis),
                              Point(leftTop.x + 120 * multiple * x_axis,
                                    leftTop.y + 75 * multiple * y_axis),
                              faceCenter, angle);

                      stage1[1] = checker3Block(
                              Point(leftTop.x + 80 * multiple * x_axis,
                                    leftTop.y + 75 * multiple * y_axis),
                              Point(leftTop.x + 90 * multiple * x_axis,
                                    leftTop.y + 100 * multiple * y_axis),
                              Point(leftTop.x + 90 * multiple * x_axis,
                                    leftTop.y + 75 * multiple * y_axis),
                              Point(leftTop.x + 110 * multiple * x_axis,
                                    leftTop.y + 100 * multiple * y_axis),
                              Point(leftTop.x + 110 * multiple * x_axis,
                                    leftTop.y + 75 * multiple * y_axis),
                              Point(leftTop.x + 120 * multiple * x_axis,
                                    leftTop.y + 100 * multiple * y_axis),
                              faceCenter, angle);*/
                      ////좌안
                      stage1[2] = checker2Block(
                              Point(leftTop.x + 40 * multiple * x_axis,
                                    leftTop.y + 40 * multiple * y_axis),
                              Point(leftTop.x + 60 * multiple * x_axis,
                                    leftTop.y + 60 * multiple * y_axis),
                              Point(leftTop.x + 40 * multiple * x_axis,
                                    leftTop.y + 60 * multiple * y_axis),
                              Point(leftTop.x + 60 * multiple * x_axis,
                                    leftTop.y + 80 * multiple * y_axis),
                              faceCenter, angle);

                      stage1[3] = checker2Block(
                              Point(leftTop.x + 62 * multiple * x_axis,
                                    leftTop.y + 40 * multiple * y_axis),
                              Point(leftTop.x + 82 * multiple * x_axis,
                                    leftTop.y + 60 * multiple * y_axis),
                              Point(leftTop.x + 62 * multiple * x_axis,
                                    leftTop.y + 60 * multiple * y_axis),
                              Point(leftTop.x + 82 * multiple * x_axis,
                                    leftTop.y + 80 * multiple * y_axis),
                              faceCenter, angle);
                      ////우안
                      stage1[4] = checker2Block(
                              Point(leftTop.x + 104 * multiple * x_axis,
                                    leftTop.y + 40 * multiple * y_axis),
                              Point(leftTop.x + 124 * multiple * x_axis,
                                    leftTop.y + 60 * multiple * y_axis),
                              Point(leftTop.x + 104 * multiple * x_axis,
                                    leftTop.y + 60 * multiple * y_axis),
                              Point(leftTop.x + 124 * multiple * x_axis,
                                    leftTop.y + 80 * multiple * y_axis),
                              faceCenter, angle);
                      stage1[5] = checker2Block(
                              Point(leftTop.x + 162 * multiple * x_axis,
                                    leftTop.y + 40 * multiple * y_axis),
                              Point(leftTop.x + 182 * multiple * x_axis,
                                    leftTop.y + 60 * multiple * y_axis),
                              Point(leftTop.x + 162 * multiple * x_axis,
                                    leftTop.y + 60 * multiple * y_axis),
                              Point(leftTop.x + 182 * multiple * x_axis,
                                    leftTop.y + 80 * multiple * y_axis),
                              faceCenter, angle);

                        tempDiff = abs(g_stage1[axisChange][size][2] - stage1[2] + g_stage1[axisChange][size][3] - stage1[3]) / normalizeStage1(2) ;

                        //얼굴과 유사한지 확인
                        if( tempDiff < 20 ) {
                            tempDiff = abs(g_stage1[axisChange][size][4] - stage1[4] + g_stage1[axisChange][size][5] - stage1[5]) / normalizeStage1(4) ;

                            if (tempDiff < 40) {
                                ////입술
                                stage1[6] = checker2Block(
                                        Point(leftTop.x + 87 * multiple * x_axis,
                                              leftTop.y + 111 * multiple * y_axis),
                                        Point(leftTop.x + 107 * multiple * x_axis,
                                              leftTop.y + 131 * multiple * y_axis),
                                        Point(leftTop.x + 87 * multiple * x_axis,
                                              leftTop.y + 131 * multiple * y_axis),
                                        Point(leftTop.x + 107 * multiple * x_axis,
                                              leftTop.y + 151 * multiple * y_axis),
                                        faceCenter, angle);
                                stage1[7] = checker2Block(
                                        Point(leftTop.x + 127 * multiple * x_axis,
                                              leftTop.y + 100 * multiple * y_axis),
                                        Point(leftTop.x + 147 * multiple * x_axis,
                                              leftTop.y + 120 * multiple * y_axis),
                                        Point(leftTop.x + 127 * multiple * x_axis,
                                              leftTop.y + 120 * multiple * y_axis),
                                        Point(leftTop.x + 147 * multiple * x_axis,
                                              leftTop.y + 140 * multiple * y_axis),
                                        faceCenter, angle);

                                tempDiff = abs(g_stage1[axisChange][size][6] - stage1[6] + g_stage1[axisChange][size][7] - stage1[7]) / normalizeStage1(6) ;

                                if ( tempDiff < 40) {
                                    /*if (stage1[0] > g_thresHold[axisChange][size][0] &&
                                        stage1[1] > g_thresHold[axisChange][size][1] &&
                                        stage1[2] < g_thresHold[axisChange][size][2] &&
                                        stage1[3] < g_thresHold[axisChange][size][3] &&
                                        stage1[4] < g_thresHold[axisChange][size][4] &&
                                        stage1[5] < g_thresHold[axisChange][size][5] &&
                                        stage1[6] < g_thresHold[axisChange][size][6] &&
                                        stage1[7] < g_thresHold[axisChange][size][7]) {
              */
                                    //눈
                                    stage2[0] = checker3Block(
                                            Point(leftTop.x + 82 * multiple * x_axis,
                                                  leftTop.y +
                                                  123 * multiple * y_axis),
                                            Point(leftTop.x + 92 * multiple * x_axis,
                                                  leftTop.y +
                                                  143 * multiple * y_axis),
                                            Point(leftTop.x + 92 * multiple * x_axis,
                                                  leftTop.y +
                                                  123 * multiple * y_axis),
                                            Point(leftTop.x + 112 * multiple * x_axis,
                                                  leftTop.y +
                                                  143 * multiple * y_axis),
                                            Point(leftTop.x + 112 * multiple * x_axis,
                                                  leftTop.y +
                                                  123 * multiple * y_axis),
                                            Point(leftTop.x + 122 * multiple * x_axis,
                                                  leftTop.y +
                                                  143 * multiple * y_axis),
                                            faceCenter, angle);
                                    //코
                                    stage2[1] = checker3Block(
                                            Point(leftTop.x + 83 * multiple * x_axis,
                                                  leftTop.y +
                                                  100 * multiple * y_axis),
                                            Point(leftTop.x + 93 * multiple * x_axis,
                                                  leftTop.y +
                                                  120 * multiple * y_axis),
                                            Point(leftTop.x + 93 * multiple * x_axis,
                                                  leftTop.y +
                                                  100 * multiple * y_axis),
                                            Point(leftTop.x + 113 * multiple * x_axis,
                                                  leftTop.y +
                                                  120 * multiple * y_axis),
                                            Point(leftTop.x + 113 * multiple * x_axis,
                                                  leftTop.y +
                                                  100 * multiple * y_axis),
                                            Point(leftTop.x + 123 * multiple * x_axis,
                                                  leftTop.y +
                                                  120 * multiple * y_axis),
                                            faceCenter, angle);

                                    //얼굴로 인식
                                    //if (g_stage2[0] < g_thresHold[axisChange][size][8] && g_stage2[0] < g_thresHold[axisChange][size][9]) {

                                    //현재 값 저장
                                    capturedFace[index].leftTop = leftTop;
                                    capturedFace[index].rightBottom = rightBottom;
                                    capturedFace[index].multiple =
                                            multiple * 1000; //double 형 오차 때문에..
                                   /* capturedFace[index].stage1[0] = stage1[0];
                                    capturedFace[index].stage1[1] = stage1[1];*/
                                    capturedFace[index].stage1[2] = stage1[2];
                                    capturedFace[index].stage1[3] = stage1[3];
                                    capturedFace[index].stage1[4] = stage1[4];
                                    capturedFace[index].stage1[5] = stage1[5];
                                    capturedFace[index].stage1[6] = stage1[6];
                                    capturedFace[index].stage1[7] = stage1[7];
                                    capturedFace[index].angle = angle;
                                    capturedFace[index].axis = 0;
                                    capturedFace[index].stage2[0] = stage2[0];
                                    capturedFace[index++].stage2[1] = stage2[1];

                                    //  }
                                    //  }
                                }
                            }
                        }


                        /*if (i == 0 || i == 1 || i == 2)
                            angle++;
                        else if (i == 3)
                            angle -= 4;
                        else if (i == 4 || i == 5 || i == 6)
                            angle--;*/

                     /* if (i == 0 )
                          angle++;
                      else if (i == 1)
                          angle -= 2;
                  }*/

                    leftTop.x += 5;
                    rightBottom.x += 5;

                }
           }

            if (axisChange == 0)
            {
                x_axis -= 0.2;
                y_axis = 1;
            }
            else if (axisChange == 1)
            {
                x_axis = 1;
                y_axis -= 0.2;
            }

        }

        int bestIndex = 0;

        if (index != 0)
            bestIndex = pickBestFace(index);
        else
            capturedFace[bestIndex].multiple = 0;


        return bestIndex;
    }


  //모델과 가장 적게 차이나는 인덱스 반환
    int pickBestFace(int num) {

        int** stage1Diff = new int*[num];
        int** stage2Diff = new int*[num];
        int result = 0;
        int highScore;
        int sum;
        int multipleIndex = 0;//multiple =1 ,0.975 ,0.95 --..
        int normalizer;

        for (int i = 0; i < num; i++) {

            multipleIndex = multipleSizeIndex(capturedFace[i].multiple);

            stage1Diff[i] = new int[MAX_INDEX_STAGE_1];
            stage2Diff[i] =new int[MAX_INDEX_STAGE_2];

            //0,1 임시 미사용 > 2 부터
            for(int k=2;k<MAX_INDEX_STAGE_1 ;k++) {
                normalizer = normalizeStage1(k);
                stage1Diff[i][k] = abs(g_stage1[capturedFace[i].axis][multipleIndex][k] - capturedFace[i].stage1[k])/normalizer;
            }
            for(int k=0;k<MAX_INDEX_STAGE_2 ;k++) {
                normalizer = normalizeStage2(k);
                stage2Diff[i][k] = abs(g_stage2[capturedFace[i].axis][multipleIndex][k] - capturedFace[i].stage2[k])/normalizer;
            }

            sum = ( + stage1Diff[i][2] + stage1Diff[i][3] + stage1Diff[i][4]
                                 + stage1Diff[i][5] + stage1Diff[i][6] + stage1Diff[i][7] + stage2Diff[i][0] + stage2Diff[i][1] ) / ((double)capturedFace[i].multiple/1000) / ((double)capturedFace[i].multiple/1000) ;


            if (i == 0)
                highScore = sum;
            else if (highScore >= sum) {
                highScore = sum;
                result = i;
            }
        }

      //LOGI(" %d %d %d %d %d %d  %d %d" , capturedFace[result].stage1[2], capturedFace[result].stage1[3] , capturedFace[result].stage1[4] , capturedFace[result].stage1[5] , capturedFace[result].stage1[6] , capturedFace[result].stage1[7],  capturedFace[result].stage2[0], capturedFace[result].stage2[1] );
      //LOGI(" %d %d %d %d %d %d" , stage1Diff[result][2],stage1Diff[result][3] ,stage1Diff[result][4] ,stage1Diff[result][5] ,stage1Diff[result][6] ,stage1Diff[result][7] );
        return result;
    }

    int normalizeStage2(int k){

        if(k == 0 || k == 1){
            return 1200;        //stage2 크기 60x20
        }

    }

    int normalizeStage1(int k){

        if(k == 0 || k == 1){
            return 250;         //stage1 0,1 번째 필터 크기 25x10
        }
        else if(k == 2 || k == 3 || k == 4 || k == 5){
            return 400;
        }
        else if(k == 6 || k == 7 ){
            return 200;
        }
    }


    void loadFaceData(int flag) {

        ifstream in;
        if(flag == AVERAGE_DATA) {
            in.open("/sdcard/Download/faceData.txt");
        }
        else {
            in.open("/sdcard/Download/eigenData.txt");
        }
        string s;
        size_t pos;

        int multipleIndex = 0;//multiple =1 ,0.975 ,0.95 --..

        for(int i=0; i<3 ;i ++) {
            g_thresHold[i] = new int *[20];
            g_stage2[i] = new int *[20];
            g_stage1[i] = new int *[20];
        }

        for(int i=0 ;i<3; i++) {
            for (multipleIndex = 0; multipleIndex < 20; multipleIndex++) {
                g_stage1[i][multipleIndex] = new int[MAX_INDEX_STAGE_1];
            }

            for (multipleIndex = 0; multipleIndex < 20; multipleIndex++) {
                g_stage2[i][multipleIndex] = new int[MAX_INDEX_STAGE_2];
            }
        }

        multipleIndex = 0;
        double multipleTmemp = 1;
        int skewIndex = 0;

        while (true) {
            in >> s;

            if (in.eof())
                break;

            if ((pos = s.find("Nose1", 0)) != string::npos) {
                g_stage1[skewIndex][multipleIndex][0] = stoi(s.replace(0, 6, ""));
            }
            else if ((pos = s.find("Nose2", 0)) != string::npos) {
                g_stage1[skewIndex][multipleIndex][1] = stoi(s.replace(0, 6, ""));
            }
            else if ((pos = s.find("EyeLeft1", 0)) != string::npos) {
                g_stage1[skewIndex][multipleIndex][2] = stoi(s.replace(0, 9, ""));
            }
            else if ((pos = s.find("EyeLeft2", 0)) != string::npos) {
                g_stage1[skewIndex][multipleIndex][3] = stoi(s.replace(0, 9, ""));
            }
            else if ((pos = s.find("Eye1", 0)) != string::npos) {
                g_stage1[skewIndex][multipleIndex][4] = stoi(s.replace(0, 5, ""));
            }
            else if ((pos = s.find("Eye2", 0)) != string::npos) {
                g_stage1[skewIndex][multipleIndex][5] = stoi(s.replace(0, 5, ""));
            }
            else if ((pos = s.find("mouth1", 0)) != string::npos) {
                g_stage1[skewIndex][multipleIndex][6] = stoi(s.replace(0, 7, ""));
            }
            else if ((pos = s.find("mouth2", 0)) != string::npos) {
                g_stage1[skewIndex][multipleIndex][7] = stoi(s.replace(0, 7, ""));
            }
            else if ((pos = s.find("stage2-1", 0)) != string::npos) {
                g_stage2[skewIndex][multipleIndex][0] = stoi(s.replace(0, 9, ""));
            }
            else if ((pos = s.find("stage2-2", 0)) != string::npos) {
                g_stage2[skewIndex][multipleIndex][1] = stoi(s.replace(0, 9, ""));
            }
            else if ((pos = s.find("multiple", 0)) != string::npos) {
                if (stod(s.replace(0, 9, "")) != multipleTmemp)
                    multipleIndex++;

                multipleTmemp = stod(s);
            }
            else if ((pos = s.find("axis", 0)) != string::npos) {
                if (stoi(s.replace(0, 5, "")) != skewIndex)
                {
                    skewIndex++;
                    multipleIndex = -1;
                }
                skewIndex = stoi(s);
            }
        }
    }


    //2개 haar filter
    int checker2Block(Point leftTop, Point rightBottom, Point sLeftTop, Point sRightBottom,
                      Point faceCenter, int angle) {

        int firstRect = 0;
        int secontRect = 0;

        for (int i = leftTop.y; i < rightBottom.y; i++)
        {
            for (int j = leftTop.x; j < rightBottom.x; j++)
            {
                firstRect += rotateAngle(j, i, faceCenter, angle);
            }
        }

        for (int i = sLeftTop.y; i < sRightBottom.y; i++)
        {
            for (int j = sLeftTop.x; j < sRightBottom.x; j++)
            {
                secontRect += rotateAngle(j, i, faceCenter, angle);
            }
        }

        return  (firstRect - secontRect);
    }

  //3개 filter
    int checker3Block(Point leftTop, Point rightBottom, Point sLeftTop, Point sRightBottom,
                      Point tLeftTop, Point tRightBottom, Point faceCenter, int angle) {

        int firstRect = 0;
        int secontRect = 0;
        int thirdRect = 0;

        for (int i = leftTop.y; i < rightBottom.y; i++)
        {
            for (int j = leftTop.x; j < rightBottom.x; j++)
            {
                firstRect += rotateAngle(j, i, faceCenter, angle);
            }
        }
        for (int i = sLeftTop.y; i < sRightBottom.y; i++)
        {
            for (int j = sLeftTop.x; j < sRightBottom.x; j++)
            {
                secontRect += rotateAngle(j, i, faceCenter, angle);
            }
        }

        for (int i = tLeftTop.y; i < tRightBottom.y; i++)
        {
            for (int j = tLeftTop.x; j < tRightBottom.x; j++)
            {
                thirdRect += rotateAngle(j, i, faceCenter, angle);
            }
        }
        return (secontRect - firstRect - thirdRect);
    }

    int rotateAngle(int inputX, int inputY, Point center, int angle) {

        Point rotated;
        double angleRadian;

        if (angle == 0)
            return ((buf[( (cameraWidth * inputY + inputX ))] ) & 0xff);

        angle *= 5;
        angleRadian = angle * 3.141592 / 180;

        rotated.x = floor(cos(angleRadian) * (inputX - center.x) - sin(angleRadian) * (inputY - center.y) + 0.5) + center.x;
        rotated.y = floor(sin(angleRadian) * (inputX - center.x) + cos(angleRadian) * (inputY - center.y) + 0.5) + center.y;

        return  ((buf[( (cameraWidth * rotated.y + rotated.x ))] ) & 0xff) ;
    }

    //현재 미사용
    void getMinStages() {

        ifstream in("/sdcard/Download/faceDataDetailed.txt");
        string s;
        size_t pos;

        int stage2[MAX_INDEX_STAGE_2] = {-465398, -465398};
        int stage1[MAX_INDEX_STAGE_1] = { 100000, 100000, -1000000, -1000000 , -1000000, -1000000, -1000000, -1000000 }; //nose,eye0,eye1,mouth
        double multipleTemp = 1;
        int multipleIndex;

        for(int i=0 ;i<3; i++) {
            for (multipleIndex = 0; multipleIndex < 20; multipleIndex++) {
                g_thresHold[i][multipleIndex] = new int[10];
            }
        }

        multipleIndex = 0;
        int skewIndex = 0;

        while (true) {
            in >> s;

            if (in.eof())
                break;

            if ((pos = s.find("Nose1", 0)) != string::npos) {
                if (stoi(s.replace(0, 6, "")) < stage1[0])
                    stage1[0] = stoi(s);
            }
            else if ((pos = s.find("Nose2", 0)) != string::npos) {
                if (stoi(s.replace(0, 6, "")) < stage1[1])
                    stage1[1] = stoi(s);
            }
            else if ((pos = s.find("EyeLeft1", 0)) != string::npos) {
                if (stoi(s.replace(0, 9, "")) > stage1[2])
                    stage1[2] = stoi(s);
            }
            else if ((pos = s.find("EyeLeft2", 0)) != string::npos) {
                if (stoi(s.replace(0, 9, "")) > stage1[3])
                    stage1[3] = stoi(s);
            }
            else if ((pos = s.find("Eye1", 0)) != string::npos) {
                if (stoi(s.replace(0, 5, "")) > stage1[4])
                    stage1[4] = stoi(s);
            }
            else if ((pos = s.find("Eye2", 0)) != string::npos) {
                if (stoi(s.replace(0, 5, "")) > stage1[5])
                    stage1[5] = stoi(s);
            }
            else if ((pos = s.find("mouth1", 0)) != string::npos) {
                if (stoi(s.replace(0, 7, "")) > stage1[6])
                    stage1[6] = stoi(s);
            }
            else if ((pos = s.find("mouth2", 0)) != string::npos) {
                if (stoi(s.replace(0, 7, "")) > stage1[7])
                    stage1[7] = stoi(s);
            }
            else if ((pos = s.find("stage2-1", 0)) != string::npos) {
                if (stoi(s.replace(0, 9, "")) > stage2[0])
                    stage2[0] = stoi(s);
            }
            else if ((pos = s.find("stage2-2", 0)) != string::npos) {
                if (stoi(s.replace(0, 9, "")) > stage2[1])
                    stage2[1] = stoi(s);
            }
            else if ((pos = s.find("multiple", 0)) != string::npos) {
                if (stod(s.replace(0, 9, "")) != multipleTemp)
                {
                    g_thresHold[skewIndex][multipleIndex][0] = stage1[0];
                    g_thresHold[skewIndex][multipleIndex][1] = stage1[1];
                    g_thresHold[skewIndex][multipleIndex][2] = stage1[2];
                    g_thresHold[skewIndex][multipleIndex][3] = stage1[3];
                    g_thresHold[skewIndex][multipleIndex][4] = stage1[4];
                    g_thresHold[skewIndex][multipleIndex][5] = stage1[5];
                    g_thresHold[skewIndex][multipleIndex][6] = stage1[6];
                    g_thresHold[skewIndex][multipleIndex][7] = stage1[7];
                    g_thresHold[skewIndex][multipleIndex][8] = stage2[0];
                    g_thresHold[skewIndex][multipleIndex][9] = stage2[1];

                    multipleIndex++;

                    stage1[0] = 100000; //쓰레기값넣기
                    stage1[1] = 100000; //쓰레기값넣기
                    stage1[2] = -1000000; //쓰레기값넣기
                    stage1[3] = -1000000; //쓰레기값넣기
                    stage1[4] = -1000000; //쓰레기값넣기
                    stage1[5] = -1000000; //쓰레기값넣기
                    stage1[6] = -1000000; //쓰레기값넣기
                    stage1[7] = -1000000; //쓰레기값넣기
                    stage2[0] = -1000000; //쓰레기값넣기
                    stage2[1] = -1000000; //쓰레기값넣기

                    //axis 변경
                    if(multipleTemp == 0.525 && stod(s) == 1){
                        skewIndex++;
                        multipleIndex = 0;
                    }

                }

                multipleTemp = stod(s);
            }
            else if ((pos = s.find("end", 0)) != string::npos) {
                g_thresHold[skewIndex][multipleIndex][0] = stage1[0];
                g_thresHold[skewIndex][multipleIndex][1] = stage1[1];
                g_thresHold[skewIndex][multipleIndex][2] = stage1[2];
                g_thresHold[skewIndex][multipleIndex][3] = stage1[3];
                g_thresHold[skewIndex][multipleIndex][4] = stage1[4];
                g_thresHold[skewIndex][multipleIndex][5] = stage1[5];
                g_thresHold[skewIndex][multipleIndex][6] = stage1[6];
                g_thresHold[skewIndex][multipleIndex][7] = stage1[7];
                g_thresHold[skewIndex][multipleIndex][8] = stage2[0];
                g_thresHold[skewIndex][multipleIndex][9] = stage2[1];
                break;
            }

        }

        for(int k=0;k<3;k++)
            for(int i=0;i<20;i++)
                LOGI("%d %d %d %d %d %d %d %d %d %d",g_thresHold[k][i][0],g_thresHold[k][i][1],g_thresHold[k][i][2],g_thresHold[k][i][3],
                     g_thresHold[k][i][4],g_thresHold[k][i][5],g_thresHold[k][i][6],g_thresHold[k][i][7],g_thresHold[k][i][8],g_thresHold[k][i][9]);

    }

    int multipleSizeIndex(int multiple){
        int size;

        if (multiple >= 995)
            size = 0;
        else if (multiple >= 970)
            size = 1;
        else if (multiple >= 945)
            size = 2;
        else if (multiple >= 920)
            size = 3;
        else if (multiple >= 895)
            size = 4;
        else if (multiple >= 870)
            size = 5;
        else if (multiple >= 845)
            size = 6;
        else if (multiple >= 820)
            size = 7;
        else if (multiple >= 795)
            size = 8;
        else if (multiple >= 770)
            size = 9;
        else if (multiple >= 745)
            size = 10;
        else if (multiple >= 720)
            size = 11;
        else if (multiple >= 695)
            size = 12;
        else if (multiple >= 670)
            size = 13;
        else if (multiple >= 645)
            size = 14;
        else if (multiple >= 620)
            size = 15;
        else if (multiple >= 595)
            size = 16;
        else if (multiple >= 570)
            size = 17;
        else if (multiple >= 545)
            size = 18;
        else if (multiple >= 495)
            size = 19;

        return size;
    }
