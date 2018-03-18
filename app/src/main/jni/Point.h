//
// Created by b on 2017-12-04.
//

#ifndef FACEDETECTION_POINT_H
#define FACEDETECTION_POINT_H

#endif //FACEDETECTION_POINT_H

class Point
{
public:
    Point();
    ~Point();
    Point(int x, int y);
    int x;
    int y;


    Point operator+=(const Point& p) {
        Point temp(x + p.x, y + p.y);
        return temp;
    }
    Point operator/=(const Point& p) {
        Point temp(x / p.x, y / p.y);
        return temp;
    }
    Point operator*=(const Point& p) {
        Point temp(x * p.x, y * p.y);
        return temp;
    }
    Point operator-=(const Point& p) {
        Point temp(x - p.x, y - p.y);
        return temp;
    }

    friend Point operator+(const Point& p1, const Point& p2);
    friend Point operator-(const Point& p1, const Point& p2);
    friend Point operator/(const Point& p1, const Point& p2);
    friend Point operator*(const Point& p1, const Point& p2);

    friend Point operator+(const Point& p1, const int& p2);
    friend Point operator*(const Point& p1, const int& p2);
    friend Point operator/(const Point& p1, const int& p2);
    friend Point operator-(const Point& p1, const int& p2);

    friend Point operator+(const Point& p1, const double& p2);
    friend Point operator*(const Point& p1, const double& p2);
    friend Point operator/(const Point& p1, const double& p2);
    friend Point operator-(const Point& p1, const double& p2);

};
Point operator/(const Point& p1, const double& p2)
{
    Point temp(p1.x / p2, p1.y / p2);
    return temp;
}
Point operator-(const Point& p1, const double& p2)
{
    Point temp(p1.x - p2, p1.y - p2);
    return temp;
}
Point operator*(const Point& p1, const double& p2)
{
    Point temp(p1.x * p2, p1.y * p2);
    return temp;
}
Point operator+(const Point& p1, const double& p2)
{
    Point temp(p1.x + p2, p1.y + p2);
    return temp;
}
Point operator/(const Point& p1, const int& p2)
{
    Point temp(p1.x / p2, p1.y / p2);
    return temp;
}
Point operator-(const Point& p1, const int& p2)
{
    Point temp(p1.x - p2, p1.y - p2);
    return temp;
}
Point operator*(const Point& p1, const int& p2)
{
    Point temp(p1.x * p2, p1.y * p2);
    return temp;
}
Point operator+(const Point& p1, const int& p2)
{
    Point temp(p1.x + p2, p1.y + p2);
    return temp;
}
Point operator+(const Point& p1, const Point& p2)
{
    Point temp(p1.x + p2.x, p1.y + p2.y);
    return temp;
}
Point operator*(const Point& p1, const Point& p2)
{
    Point temp(p1.x * p2.x, p1.y * p2.y);
    return temp;
}

Point operator/(const Point& p1, const Point& p2)
{
    Point temp(p1.x / p2.x, p1.y / p2.y);
    return temp;
}
Point operator-(const Point& p1, const Point& p2)
{
    Point temp(p1.x - p2.x, p1.y - p2.y);
    return temp;
}


Point::Point(int _x, int _y)
{
    x = _x;
    y = _y;
}


Point::Point()
{
    x = 0;
    y = 0;
}


Point::~Point()
{
}
