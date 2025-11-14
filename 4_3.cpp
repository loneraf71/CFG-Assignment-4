#include <GL/freeglut.h>
#include <GL/glu.h>
#include <cmath>
#include <vector>
#include <iostream>
#include <fstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
};

static Vec3 crossp(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

static float dotp(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 normalize(const Vec3& v) {
    float L = sqrtf(dotp(v, v));
    return (L > 1e-6f) ? Vec3(v.x / L, v.y / L, v.z / L) : v;
}

Vec3 ctrl[4][4];

static bool loadControlPointsFromFile(const char* fname) {
    std::ifstream in(fname);
    if (!in.is_open()) return false;
    for (int j = 0; j < 4; j++)
        for (int i = 0; i < 4; i++) {
            float x, y, z;
            if (!(in >> x >> y >> z)) return false;
            ctrl[i][j] = Vec3(x, y, z);
        }
    return true;
}

static void setDefaultControlPoints() {
    float d[16][3] = {
        {-1.5f,-1.5f,0},{-0.5f,-1.5f,0},{0.5f,-1.5f,0},{1.5f,-1.5f,0},
        {-1.5f,-0.5f,0},{-0.5f,-0.5f,1.2f},{0.5f,-0.5f,1.2f},{1.5f,-0.5f,0},
        {-1.5f,0.5f,0},{-0.5f,0.5f,1.2f},{0.5f,0.5f,1.2f},{1.5f,0.5f,0},
        {-1.5f,1.5f,0},{-0.5f,1.5f,0},{0.5f,1.5f,0},{1.5f,1.5f,0}
    };
    for (int j = 0; j < 4; j++)
        for (int i = 0; i < 4; i++)
            ctrl[i][j] = Vec3(d[j * 4 + i][0], d[j * 4 + i][1], d[j * 4 + i][2]);
}

static void bernstein3(float u, float B[4]) {
    float om = 1 - u;
    B[0] = om * om * om;
    B[1] = 3 * u * om * om;
    B[2] = 3 * u * u * om;
    B[3] = u * u * u;
}

static void bernstein3_deriv(float u, float dB[4]) {
    float om = 1 - u;
    dB[0] = -3 * om * om;
    dB[1] = 3 * om * om - 6 * u * om;
    dB[2] = 6 * u * om - 3 * u * u;
    dB[3] = 3 * u * u;
}

static Vec3 evalP(float u, float v) {
    float Bu[4], Bv[4];
    bernstein3(u, Bu);
    bernstein3(v, Bv);
    Vec3 P;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            P = P + ctrl[i][j] * (Bu[i] * Bv[j]);
    return P;
}

static Vec3 evalPu(float u, float v) {
    float dBu[4], Bv[4];
    bernstein3_deriv(u, dBu);
    bernstein3(v, Bv);
    Vec3 P;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            P = P + ctrl[i][j] * (dBu[i] * Bv[j]);
    return P;
}

static Vec3 evalPv(float u, float v) {
    float Bu[4], dBv[4];
    bernstein3(u, Bu);
    bernstein3_deriv(v, dBv);
    Vec3 P;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            P = P + ctrl[i][j] * (Bu[i] * dBv[j]);
    return P;
}

int RES = 12;
bool useTex = true;
float camYawDeg = 45.0f, camPitchDeg = 20.0f, camDistVal = 6.0f;
GLuint tex;

static void makeTex(int N = 256) {
    std::vector<unsigned char> img(N * N * 3);
    for (int j = 0; j < N; j++) {
        for (int i = 0; i < N; i++) {
            float u = i / float(N - 1);
            float v = j / float(N - 1);
            // Исправление переполнения
            unsigned char R = static_cast<unsigned char>(255.0f * u);
            unsigned char G = static_cast<unsigned char>(255.0f * v);
            unsigned char B = static_cast<unsigned char>(255.0f * (1.0f - u));
            int idx = (j * N + i) * 3;
            img[idx] = R;
            img[idx + 1] = G;
            img[idx + 2] = B;
        }
    }
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, N, N, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static void drawPatch() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    // Настройка материала
    GLfloat mat_diffuse[] = { 0.7f, 0.7f, 0.7f, 1.0f };
    GLfloat mat_specular[] = { 0.6f, 0.6f, 0.6f, 1.0f };
    GLfloat mat_shininess[] = { 32.0f };
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);

    // Настройка света
    GLfloat light_pos[] = { 5.0f, 5.0f, 5.0f, 1.0f };
    GLfloat light_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat light_ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);

    if (useTex) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }
    else {
        glDisable(GL_TEXTURE_2D);
    }

    // Рисуем патч как сетку треугольников
    glBegin(GL_TRIANGLES);
    for (int j = 0; j < RES - 1; j++) {
        for (int i = 0; i < RES - 1; i++) {
            float u1 = i / float(RES - 1);
            float u2 = (i + 1) / float(RES - 1);
            float v1 = j / float(RES - 1);
            float v2 = (j + 1) / float(RES - 1);

            // Вычисляем 4 угловые точки
            Vec3 p00 = evalP(u1, v1);
            Vec3 p10 = evalP(u2, v1);
            Vec3 p01 = evalP(u1, v2);
            Vec3 p11 = evalP(u2, v2);

            // Вычисляем нормали
            Vec3 pu00 = evalPu(u1, v1);
            Vec3 pv00 = evalPv(u1, v1);
            Vec3 n00 = normalize(crossp(pu00, pv00));

            Vec3 pu10 = evalPu(u2, v1);
            Vec3 pv10 = evalPv(u2, v1);
            Vec3 n10 = normalize(crossp(pu10, pv10));

            Vec3 pu01 = evalPu(u1, v2);
            Vec3 pv01 = evalPv(u1, v2);
            Vec3 n01 = normalize(crossp(pu01, pv01));

            Vec3 pu11 = evalPu(u2, v2);
            Vec3 pv11 = evalPv(u2, v2);
            Vec3 n11 = normalize(crossp(pu11, pv11));

            // Первый треугольник
            glNormal3f(n00.x, n00.y, n00.z);
            glTexCoord2f(u1, v1); glVertex3f(p00.x, p00.y, p00.z);

            glNormal3f(n10.x, n10.y, n10.z);
            glTexCoord2f(u2, v1); glVertex3f(p10.x, p10.y, p10.z);

            glNormal3f(n11.x, n11.y, n11.z);
            glTexCoord2f(u2, v2); glVertex3f(p11.x, p11.y, p11.z);

            // Второй треугольник
            glNormal3f(n00.x, n00.y, n00.z);
            glTexCoord2f(u1, v1); glVertex3f(p00.x, p00.y, p00.z);

            glNormal3f(n11.x, n11.y, n11.z);
            glTexCoord2f(u2, v2); glVertex3f(p11.x, p11.y, p11.z);

            glNormal3f(n01.x, n01.y, n01.z);
            glTexCoord2f(u1, v2); glVertex3f(p01.x, p01.y, p01.z);
        }
    }
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

static void display() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);
    gluPerspective(45.0, static_cast<double>(w) / static_cast<double>(h), 0.1, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Позиция камеры
    float cy = camDistVal * sinf(camPitchDeg * static_cast<float>(M_PI) / 180.0f);
    float cx = camDistVal * cosf(camPitchDeg * static_cast<float>(M_PI) / 180.0f) *
        cosf(camYawDeg * static_cast<float>(M_PI) / 180.0f);
    float cz = camDistVal * cosf(camPitchDeg * static_cast<float>(M_PI) / 180.0f) *
        sinf(camYawDeg * static_cast<float>(M_PI) / 180.0f);

    gluLookAt(cx, cy, cz, 0, 0, 0, 0, 1, 0);

    drawPatch();

    glutSwapBuffers();
}

static void keys(unsigned char k, int, int) {
    if (k == 27 || k == 'q') exit(0);
    if (k == 'w') camDistVal = std::max(0.5f, camDistVal - 0.3f);
    if (k == 's') camDistVal += 0.3f;
    if (k == 't') {
        useTex = !useTex;
        std::cout << "Texture " << (useTex ? "ON" : "OFF") << "\n";
    }
    if (k == '+') { RES = std::min(50, RES + 2); std::cout << "Resolution: " << RES << "\n"; }
    if (k == '-') { RES = std::max(4, RES - 2); std::cout << "Resolution: " << RES << "\n"; }
    glutPostRedisplay();
}

static void special(int key, int, int) {
    if (key == GLUT_KEY_LEFT) camYawDeg -= 5;
    if (key == GLUT_KEY_RIGHT) camYawDeg += 5;
    if (key == GLUT_KEY_UP) camPitchDeg = std::min(89.0f, camPitchDeg + 5);
    if (key == GLUT_KEY_DOWN) camPitchDeg = std::max(-89.0f, camPitchDeg - 5);
    glutPostRedisplay();
}

static void init() {
    makeTex();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);
}

int main(int argc, char** argv) {
    if (!loadControlPointsFromFile("patchPoints.txt"))
        setDefaultControlPoints();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(1000, 700);
    glutCreateWindow("Bezier Patch - Fixed Version");

    init();

    glutDisplayFunc(display);
    glutKeyboardFunc(keys);
    glutSpecialFunc(special);

    std::cout << "Controls:\n"
        << "  Arrow keys: rotate camera\n"
        << "  W/S: zoom in/out\n"
        << "  T: toggle texture\n"
        << "  +/-: increase/decrease resolution\n"
        << "  Q or Esc: quit\n";

    glutMainLoop();
    return 0;
}