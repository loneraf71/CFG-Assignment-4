#include <GL/glut.h>
#include <cmath>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace std;

struct Vec3 {
    float x, y, z;
    Vec3() :x(0), y(0), z(0) {}
    Vec3(float X, float Y, float Z) :x(X), y(Y), z(Z) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
};

static inline Vec3 crossp(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x);
}
static inline float dotp(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
static inline float len(const Vec3& v) { return sqrtf(dotp(v, v)); }
static inline Vec3 normalize(const Vec3& v) {
    float L = len(v);
    if (L == 0.0f) return Vec3(0, 0, 1);
    return v * (1.0f / L);
}

// Control points
Vec3 ctrl[4][4];

int selectedIndex = 0; // 0..15

int res = 10; // initial 10x10 resolution

// Camera spherical coords
float camDist = 6.0f;
float camAzimuth = 45.0f;
float camElevation = 20.0f;

// computed mesh
struct Tri {
    Vec3 v0, v1, v2;
    Vec3 normal;
    Vec3 color;
};
vector<Tri> triangles;

// material and light
Vec3 lightColor = Vec3(1.0f, 1.0f, 1.0f);
Vec3 kd = Vec3(0.7f, 0.5f, 0.2f);


Vec3 patchCenter(0, 0, 0);

static bool loadControlPointsFromFile(const char* fname) {
    ifstream in(fname);
    if (!in.is_open()) return false;
    vector<Vec3> readPts;
    for (int i = 0; i < 16; i++) {
        float x, y, z;
        if (!(in >> x >> y >> z)) break;
        readPts.emplace_back(x, y, z);
    }
    if (readPts.size() != 16) return false;

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int idx = r * 4 + c;
            ctrl[c][r] = readPts[idx]; // ctrl[x][y]
        }
    }
    return true;
}

// pleasant shape
static void setDefaultControlPoints() {
    float def[16][3] = {
        {-1.5f,-1.5f, 0.0f}, {-0.5f,-1.5f, 0.0f}, {0.5f,-1.5f, 0.0f}, {1.5f,-1.5f, 0.0f},
        {-1.5f,-0.5f, 0.0f}, {-0.5f,-0.5f, 1.5f}, {0.5f,-0.5f, 1.5f}, {1.5f,-0.5f, 0.0f},
        {-1.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 1.5f}, {0.5f, 0.5f, 1.5f}, {1.5f, 0.5f, 0.0f},
        {-1.5f, 1.5f, 0.0f}, {-0.5f, 1.5f, 0.0f}, {0.5f, 1.5f, 0.0f}, {1.5f, 1.5f, 0.0f}
    };
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int idx = r * 4 + c;
            ctrl[c][r] = Vec3(def[idx][0], def[idx][1], def[idx][2]);
        }
    }
}

static void computePatchCenter() {
    Vec3 sum(0, 0, 0);
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) sum = sum + ctrl[i][j];
    patchCenter = sum * (1.0f / 16.0f);
}

// Bernstein basis n=3
static void bernstein3(float u, float B[4]) {
    float um = 1.0f - u;
    // B0 = (1-u)^3
    // B1 = 3u(1-u)^2
    // B2 = 3u^2(1-u)
    // B3 = u^3
    B[0] = um * um * um;
    B[1] = 3.0f * u * um * um;
    B[2] = 3.0f * u * u * um;
    B[3] = u * u * u;
}

// Evaluate patch point P(u,v) iteratively
static Vec3 evaluatePatchPt(float u, float v) {
    float Bu[4], Bv[4];
    bernstein3(u, Bu);
    bernstein3(v, Bv);
    Vec3 P(0, 0, 0);
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float b = Bu[i] * Bv[j];
            P = P + ctrl[i][j] * b;
        }
    }
    return P;
}

// Build mesh (triangles) 
static void buildMesh() {
    triangles.clear();
    int N = res;

    vector<vector<Vec3>> grid(N + 1, vector<Vec3>(N + 1));
    for (int v = 0; v <= N; v++) {
        float fv = static_cast<float>(v) / static_cast<float>(N);
        for (int u = 0; u <= N; u++) {
            float fu = static_cast<float>(u) / static_cast<float>(N);
            grid[u][v] = evaluatePatchPt(fu, fv);
        }
    }

    // create triangles: each cell two triangles
    for (int v = 0; v < N; v++) {
        for (int u = 0; u < N; u++) {
            Vec3 p00 = grid[u][v];
            Vec3 p10 = grid[u + 1][v];
            Vec3 p01 = grid[u][v + 1];
            Vec3 p11 = grid[u + 1][v + 1];
            // triangle 1
            Tri t1;
            t1.v0 = p00; t1.v1 = p10; t1.v2 = p11;
            Vec3 e1 = t1.v1 - t1.v0;
            Vec3 e2 = t1.v2 - t1.v0;
            t1.normal = normalize(crossp(e1, e2));
            // triangle 2
            Tri t2;
            t2.v0 = p00; t2.v1 = p11; t2.v2 = p01;
            e1 = t2.v1 - t2.v0;
            e2 = t2.v2 - t2.v0;
            t2.normal = normalize(crossp(e1, e2));
            triangles.push_back(t1);
            triangles.push_back(t2);
        }
    }

}

static void indexToCtrlCoord(int idx, int& cx, int& cy) {
    if (idx < 0) idx = 0; if (idx > 15) idx = 15;
    cy = idx / 4; cx = idx % 4;
}

// keyboard and interaction
static void adjustSelectedControlPoint(float dx, float dy, float dz) {
    int cx, cy;
    indexToCtrlCoord(selectedIndex, cx, cy);
    ctrl[cx][cy].x += dx;
    ctrl[cx][cy].y += dy;
    ctrl[cx][cy].z += dz;
    computePatchCenter();
    buildMesh();
}

static void glutDisplay() {
    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // Setup projection and camera
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = static_cast<float>(glutGet(GLUT_WINDOW_WIDTH)) / static_cast<float>(glutGet(GLUT_WINDOW_HEIGHT));
    gluPerspective(45.0, aspect, 0.1, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // compute camera position in Cartesian coords
    float az = camAzimuth * static_cast<float>(M_PI) / 180.0f;
    float el = camElevation * static_cast<float>(M_PI) / 180.0f;
    Vec3 camPos;
    camPos.x = patchCenter.x + camDist * cosf(el) * cosf(az);
    camPos.y = patchCenter.y + camDist * sinf(el);
    camPos.z = patchCenter.z + camDist * cosf(el) * sinf(az);

    gluLookAt(camPos.x, camPos.y, camPos.z,
        patchCenter.x, patchCenter.y, patchCenter.z,
        0.0, 1.0, 0.0);

    // set light at camera position 
    Vec3 lightPos = camPos;

    glPushMatrix();
    glTranslatef(patchCenter.x, patchCenter.y, patchCenter.z);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    // X axis (red)
    glColor3f(1.0f, 0.0f, 0.0f); glVertex3f(0, 0, 0); glVertex3f(0.5f, 0, 0);
    // Y axis (green)
    glColor3f(0.0f, 1.0f, 0.0f); glVertex3f(0, 0, 0); glVertex3f(0, 0.5f, 0);
    // Z axis (blue)
    glColor3f(0.0f, 0.0f, 1.0f); glVertex3f(0, 0, 0); glVertex3f(0, 0, 0.5f);
    glEnd();
    glPopMatrix();

    // draw patch triangles with per-triangle color
    glShadeModel(GL_FLAT);
    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < triangles.size(); i++) {
        Tri& t = triangles[i];
        // triangle center
        Vec3 center = (t.v0 + t.v1 + t.v2) * (1.0f / 3.0f);
        Vec3 L = normalize(lightPos - center);
        float ndotl = dotp(t.normal, L);
        if (ndotl < 0) ndotl = 0;
        Vec3 col = Vec3(kd.x * lightColor.x * ndotl,
            kd.y * lightColor.y * ndotl,
            kd.z * lightColor.z * ndotl);

        Vec3 ambient = Vec3(0.08f, 0.08f, 0.08f);
        col = col + ambient;
        // clamp
        col.x = fminf(1.0f, col.x); col.y = fminf(1.0f, col.y); col.z = fminf(1.0f, col.z);
        glColor3f(col.x, col.y, col.z);
        // supply normal for correctness 
        glNormal3f(t.normal.x, t.normal.y, t.normal.z);
        glVertex3f(t.v0.x, t.v0.y, t.v0.z);
        glVertex3f(t.v1.x, t.v1.y, t.v1.z);
        glVertex3f(t.v2.x, t.v2.y, t.v2.z);
    }
    glEnd();

    // draw control points (GL_POINTS)
    glPointSize(8.0f);
    glBegin(GL_POINTS);
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int idx = y * 4 + x;
            if (idx == selectedIndex) {
                glPointSize(12.0f);
                glColor3f(1.0f, 1.0f, 0.0f); // highlight
            }
            else {
                glPointSize(6.0f);
                glColor3f(0.9f, 0.9f, 0.9f);
            }
            // draw actual point
            glVertex3f(ctrl[x][y].x, ctrl[x][y].y, ctrl[x][y].z);
        }
    }
    glEnd();

    // draw lines connecting control points in grid
    glLineWidth(1.5f);
    glColor3f(0.6f, 0.6f, 0.6f);
    // horizontal lines x
    for (int y = 0; y < 4; y++) {
        glBegin(GL_LINE_STRIP);
        for (int x = 0; x < 4; x++) glVertex3f(ctrl[x][y].x, ctrl[x][y].y, ctrl[x][y].z);
        glEnd();
    }
    // vertical lines y
    for (int x = 0; x < 4; x++) {
        glBegin(GL_LINE_STRIP);
        for (int y = 0; y < 4; y++) glVertex3f(ctrl[x][y].x, ctrl[x][y].y, ctrl[x][y].z);
        glEnd();
    }

    // HUD text
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    int windowWidth = glutGet(GLUT_WINDOW_WIDTH);
    int windowHeight = glutGet(GLUT_WINDOW_HEIGHT);
    glOrtho(0, windowWidth, 0, windowHeight, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glColor3f(1, 1, 1);
    char buf[256];
    sprintf_s(buf, sizeof(buf), "res = %d  (use +/-)   selected = %d (0-9,a-f)  move: j/l i/k u/o  reset: r  quit: q/esc", res, selectedIndex);
    glRasterPos2i(10, windowHeight - 20);
    for (char* c = buf; *c; c++) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *c);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glutSwapBuffers();
}

static void glutIdle() {
    glutPostRedisplay();
}

static void specialKeys(int key, int x, int y) {
    const float turnStep = 4.0f;
    const float zoomStep = 0.5f;
    switch (key) {
    case GLUT_KEY_LEFT: camAzimuth -= turnStep; break;
    case GLUT_KEY_RIGHT: camAzimuth += turnStep; break;
    case GLUT_KEY_UP: camElevation += turnStep; if (camElevation > 89) camElevation = 89; break;
    case GLUT_KEY_DOWN: camElevation -= turnStep; if (camElevation < -89) camElevation = -89; break;
    }
    glutPostRedisplay();
}

static void keyboard(unsigned char key, int x, int y) {
    switch (key) {
    case 27: case 'q': exit(0); break;
    case 'r': // reset view
        camDist = 6.0f; camAzimuth = 45.0f; camElevation = 20.0f;
        computePatchCenter(); buildMesh();
        break;
    case '+': res = min(100, res + 1); buildMesh(); break;
    case '-': res = max(1, res - 1); buildMesh(); break;
        // select control points: '0'..'9' then 'a'..'f'
    case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
        selectedIndex = key - '0'; break;
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
        selectedIndex = 10 + (key - 'a'); break;
    case '[': selectedIndex = (selectedIndex + 15) % 16; break; // prev
    case ']': selectedIndex = (selectedIndex + 1) % 16; break; // next
        // move selected point
        // j/l -> -x/+x  i/k -> +y/-y   u/o -> +z/-z
    case 'j': adjustSelectedControlPoint(-0.05f, 0, 0); break;
    case 'l': adjustSelectedControlPoint(+0.05f, 0, 0); break;
    case 'i': adjustSelectedControlPoint(0, +0.05f, 0); break;
    case 'k': adjustSelectedControlPoint(0, -0.05f, 0); break;
    case 'u': adjustSelectedControlPoint(0, 0, +0.05f); break;
    case 'o': adjustSelectedControlPoint(0, 0, -0.05f); break;
        // camera zoom in/out
    case 'w': camDist = max(1.2f, camDist - 0.4f); break;
    case 's': camDist = min(50.0f, camDist + 0.4f); break;
        // helpful debug: print control point coords
    case 'p': {
        printf("Control points:\n");
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                int idx = y * 4 + x;
                printf("%2d: (%.3f, %.3f, %.3f)\n", idx, ctrl[x][y].x, ctrl[x][y].y, ctrl[x][y].z);
            }
        }
        break;
    }
    }
    glutPostRedisplay();
}

int main(int argc, char** argv) {

    bool loaded = loadControlPointsFromFile("patchPoints.txt");
    if (!loaded) setDefaultControlPoints();
    computePatchCenter();
    buildMesh();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(900, 700);
    glutCreateWindow(" Bezier Patch Task1");

    glEnable(GL_POINT_SMOOTH);
    glPointSize(8.0f);
    glEnable(GL_NORMALIZE);

    glutDisplayFunc(glutDisplay);
    glutIdleFunc(glutIdle);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);

    cout << "Controls:\n";
    cout << "  Select control point: keys 0-9 and a-f (a->10 ... f->15). Also '[' and ']' cycle.\n";
    cout << "  Move selected point: j/l (-x/+x), i/k (+y/-y), u/o (+z/-z)\n";
    cout << "  Increase/decrease sampling: + / -\n";
    cout << "  Camera rotate: arrow keys  Zoom: w (in) s (out)\n";
    cout << "  Reset view: r   Quit: q or Esc\n";
    cout << "  Print control points: p\n";
    cout << "  Default control points will be used unless patchPoints.txt is present.\n";

    glutMainLoop();
    return 0;
}