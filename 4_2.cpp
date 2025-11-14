#include <GL/glut.h>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <iostream>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace std;

// Window size
int winW = 900, winH = 700;

// Camera (spherical)
float camAz = 30.0f;
float camEl = 10.0f;
float camDist = 8.0f;
float camCenterX = 0.0f, camCenterY = 0.0f, camCenterZ = 0.0f;

bool useAA = true;

// Object diffuse colors (rgb)
float objColor[3][3] = {
    {0.8f, 0.2f, 0.2f}, // obj 0
    {0.2f, 0.8f, 0.2f}, // obj 1
    {0.2f, 0.2f, 0.8f}  // obj 2
};

// Unique pick colors
unsigned char pickColorBytes[3][3] = {
    { 10,  20,  30}, // id 0
    { 40,  50,  60}, // id 1
    { 70,  80,  90}  // id 2
};

static void randizeObjectColor(int id) {
    objColor[id][0] = 0.2f + 0.8f * (rand() / static_cast<float>(RAND_MAX));
    objColor[id][1] = 0.2f + 0.8f * (rand() / static_cast<float>(RAND_MAX));
    objColor[id][2] = 0.2f + 0.8f * (rand() / static_cast<float>(RAND_MAX));
}

static void setupCameraAndLight() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(55.0, static_cast<double>(winW) / static_cast<double>(winH), 0.1, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float az = camAz * static_cast<float>(M_PI) / 180.0f;
    float el = camEl * static_cast<float>(M_PI) / 180.0f;
    float cx = camCenterX, cy = camCenterY, cz = camCenterZ;
    float camX = cx + camDist * cosf(el) * cosf(az);
    float camY = cy + camDist * sinf(el);
    float camZ = cz + camDist * cosf(el) * sinf(az);
    gluLookAt(camX, camY, camZ, cx, cy, cz, 0.0f, 1.0f, 0.0f);

    GLfloat lightPos[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    GLfloat lightDiffuse[4] = { 1.0f,1.0f,1.0f,1.0f };
    GLfloat lightSpecular[4] = { 0.6f,0.6f,0.6f,1.0f };
    GLfloat lightAmbient[4] = { 0.25f, 0.25f, 0.25f, 1.0f };

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
}

static void drawScene(bool pickMode = false) {
    if (pickMode) {
        glDisable(GL_LIGHTING);
        glShadeModel(GL_FLAT);
        glDisable(GL_DITHER);
        // Убираем сглаживание для точного пикинга
        glDisable(GL_LINE_SMOOTH);
        glDisable(GL_POLYGON_SMOOTH);
        glDisable(GL_BLEND);
    }
    else {
        glEnable(GL_LIGHTING);
        glShadeModel(GL_SMOOTH);
        glEnable(GL_DITHER);
        if (useAA) {
            glEnable(GL_LINE_SMOOTH);
            glEnable(GL_POLYGON_SMOOTH);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        else {
            glDisable(GL_LINE_SMOOTH);
            glDisable(GL_POLYGON_SMOOTH);
            glDisable(GL_BLEND);
        }
    }

    for (int id = 0; id < 3; ++id) {
        glPushMatrix();

        if (id == 0) glTranslatef(-2.2f, 0.0f, 0.0f);
        if (id == 1) glTranslatef(0.0f, 0.0f, 0.0f);
        if (id == 2) glTranslatef(2.2f, 0.0f, 0.0f);

        glRotatef(-20.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(static_cast<float>(id * 30), 0.0f, 1.0f, 0.0f);

        if (pickMode) {
            glColor3ub(pickColorBytes[id][0], pickColorBytes[id][1], pickColorBytes[id][2]);
            if (id == 0) glutSolidSphere(0.9, 48, 48);
            else if (id == 1) glutSolidTorus(0.25, 0.85, 48, 48);
            else glutSolidTeapot(0.8);
        }
        else {
            GLfloat diffuse[4] = { objColor[id][0], objColor[id][1], objColor[id][2], 1.0f };
            GLfloat spec[4] = { 0.3f, 0.3f, 0.3f, 1.0f };
            GLfloat ambient[4] = { 0.08f, 0.08f, 0.08f, 1.0f };
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
            glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
            glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);

            if (id == 0) glutSolidSphere(0.9, 48, 48);
            else if (id == 1) glutSolidTorus(0.25, 0.85, 48, 48);
            else glutSolidTeapot(0.8);
        }

        glPopMatrix();
    }
}

static void pickAt(int mx, int my) {
    // Сохраняем текущий буфер
    glDrawBuffer(GL_BACK);
    glReadBuffer(GL_BACK);

    int readY = winH - 1 - my;
    unsigned char pixel[3] = { 0,0,0 };

    // Временно отключаем сглаживание для точного пикинга
    bool wasAA = useAA;
    useAA = false;

    // Рендерим сцену в режиме пикинга
    glViewport(0, 0, winW, winH);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    setupCameraAndLight();
    drawScene(true);

    glFlush();
    glFinish();

    // Читаем пиксель
    glReadPixels(mx, readY, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);

    // Восстанавливаем настройки
    useAA = wasAA;

    cout << "Picked color = ("
        << static_cast<int>(pixel[0]) << ", "
        << static_cast<int>(pixel[1]) << ", "
        << static_cast<int>(pixel[2]) << ")\n";

    int picked = -1;
    // Сравниваем с нашими цветами пикинга
    for (int id = 0; id < 3; ++id) {
        if (pixel[0] == pickColorBytes[id][0] &&
            pixel[1] == pickColorBytes[id][1] &&
            pixel[2] == pickColorBytes[id][2]) {
            picked = id;
            break;
        }
    }

    if (picked >= 0) {
        // Изменяем цвет выбранного объекта
        objColor[picked][0] = static_cast<float>(rand()) / RAND_MAX;
        objColor[picked][1] = static_cast<float>(rand()) / RAND_MAX;
        objColor[picked][2] = static_cast<float>(rand()) / RAND_MAX;

        cout << "Picked object " << picked
            << " new color = ("
            << objColor[picked][0] << ", "
            << objColor[picked][1] << ", "
            << objColor[picked][2] << ")\n";

        glutPostRedisplay();
    }
    else {
        cout << "No object picked (background)\n";
    }
}

static void display() {
    glViewport(0, 0, winW, winH);

    // Настройка сглаживания
    if (useAA) {
        glEnable(GL_LINE_SMOOTH);
        glEnable(GL_POLYGON_SMOOTH);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else {
        glDisable(GL_LINE_SMOOTH);
        glDisable(GL_POLYGON_SMOOTH);
        glDisable(GL_BLEND);
    }

    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    setupCameraAndLight();

    // Рисуем оси в центре для ориентира
    glPushMatrix();
    glTranslatef(camCenterX, camCenterY, camCenterZ);
    glDisable(GL_LIGHTING);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glColor3f(1, 0, 0); glVertex3f(0, 0, 0); glVertex3f(0.8f, 0, 0);
    glColor3f(0, 1, 0); glVertex3f(0, 0, 0); glVertex3f(0, 0.8f, 0);
    glColor3f(0, 0, 1); glVertex3f(0, 0, 0); glVertex3f(0, 0, 0.8f);
    glEnd();
    glPopMatrix();

    // Рисуем основную сцену
    drawScene(false);

    // HUD (информация на экране)
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, winW, 0, winH, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_LIGHTING);
    glColor3f(1, 1, 1);

    string hud = "AA: (a) " + string(useAA ? "ON" : "OFF") +
        "     Click to pick object     Camera: arrow keys (rotate), w/s zoom, r reset";
    glRasterPos2i(8, winH - 18);
    for (char c : hud) {
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, c);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glutSwapBuffers();
}

static void reshape(int w, int h) {
    winW = max(1, w);
    winH = max(1, h);
    glViewport(0, 0, winW, winH);
    glutPostRedisplay();
}

static void keyboard(unsigned char key, int x, int y) {
    switch (key) {
    case 27:
    case 'q':
        exit(0);
        break;
    case 'r':
        camAz = 30.0f;
        camEl = 10.0f;
        camDist = 8.0f;
        camCenterX = camCenterY = camCenterZ = 0.0f;
        break;
    case 'a':
        useAA = !useAA;
        cout << "Anti-aliasing " << (useAA ? "ON" : "OFF") << "\n";
        break;
    case 'w':
        camDist = fmaxf(1.0f, camDist - 0.4f);
        break;
    case 's':
        camDist = fminf(50.0f, camDist + 0.4f);
        break;
    case 'p':
        for (int i = 0; i < 3; i++) {
            cout << "obj " << i << " color = "
                << objColor[i][0] << ", "
                << objColor[i][1] << ", "
                << objColor[i][2] << "\n";
        }
        break;
    }
    glutPostRedisplay();
}

static void specialKey(int key, int x, int y) {
    switch (key) {
    case GLUT_KEY_LEFT:
        camAz -= 4.0f;
        break;
    case GLUT_KEY_RIGHT:
        camAz += 4.0f;
        break;
    case GLUT_KEY_UP:
        camEl = min(89.0f, camEl + 4.0f);
        break;
    case GLUT_KEY_DOWN:
        camEl = max(-89.0f, camEl - 4.0f);
        break;
    }
    glutPostRedisplay();
}

static void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        pickAt(x, y);
    }
}

static void initGL() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glDisable(GL_COLOR_MATERIAL);

    // Начальные настройки сглаживания
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

    srand(static_cast<unsigned int>(time(NULL)));
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);

    // Используем двойную буферизацию и буфер глубины
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(winW, winH);
    glutCreateWindow("Object Picking - Simple Version");

    cout << "OpenGL version: " << glGetString(GL_VERSION) << endl;

    initGL();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKey);
    glutMouseFunc(mouse);

    cout << "Controls:\n";
    cout << "  Arrow keys: rotate camera\n";
    cout << "  w/s: zoom in/out\n";
    cout << "  r: reset view\n";
    cout << "  a: toggle anti-aliasing\n";
    cout << "  Click left mouse on objects to pick and randomize their color.\n";
    cout << "  p: print current object colors\n";

    glutMainLoop();
    return 0;
}