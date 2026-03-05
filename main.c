#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 600
#define PI 3.14159265359

#// Scrolling durations (ms)
#define DOWN_DURATION_MS 20000
#define UP_DURATION_MS 20000
#define UP_SPEED_FACTOR 1.5f

// Texture variables
GLuint backgroundTexture;
unsigned char* backgroundImage = NULL;
int bgWidth, bgHeight, bgChannels;

// Runtime window and viewport (for responsive scaling)
int windowWidth = WINDOW_WIDTH;
int windowHeight = WINDOW_HEIGHT;
int viewX = 0, viewY = 0, viewW = WINDOW_WIDTH, viewH = WINDOW_HEIGHT;
float viewScale = 1.0f; // logical -> pixel scale

// Hook control variables
float hookX = WINDOW_WIDTH / 2.0f;
float hookY = 320.0f;
int hookControlActive = 0; // 1 = mouse control enabled, 0 = disabled
// Function prototypes
void drawBackground(void);
void loadBackgroundTexture(void);
void updateScroll(int value);
// Scrolling variables
float bgScrollOffset = 0.0f;
int bgScrollDirection = 1; // 1 = down, -1 = up, 0 = stopped
float bgScrollSpeed = 2.0f; // pixels per frame
int bgScrollTimer = 0; // ms elapsed
int bgScrollActive = 1; // 1 = scrolling, 0 = stopped
int screen = 0; // 0 = intro, 1 = fisherman/boat, 2 = game
float playBtnAnim = 0.0f; // 0 = idle, 0..1 = animating
float playBtnScale = 1.0f, playBtnAlpha = 1.0f, gameAlpha = 0.0f;
int animatingTransition = 0;
// Intro screen interaction
int introMouseX = 0, introMouseY = 0; // updated by passive motion
float introPulse = 0.0f;
float introPulseScale = 1.0f;

// Fish for game screen
#define GAME_FISH_COUNT 6
typedef struct { float x, y, speed, r, g, b; int active; } GameFish;
GameFish gameFish[GAME_FISH_COUNT];
float waveOffset = 0;

// Score variable
int score = 0;

// Respawn delay for caught fish
#define FISH_RESPAWN_DELAY 1000 // ms
int fishRespawnTimers[GAME_FISH_COUNT] = {0};

// Popup overlay state
int popupActive = 0;        // 1 = popup visible (animating or shown)
float popupAlpha = 0.0f;    // 0..1 fade-in alpha
int popupAnimating = 0;     // 1 = fade-in in progress


// Draw a filled circle
void drawFilledCircle(float x, float y, float radius, int segments) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for(int i = 0; i <= segments; i++) {
        float angle = 2.0f * PI * i / segments;
        glVertex2f(x + cos(angle) * radius, y + sin(angle) * radius);
    }
    glEnd();
}

// Convert window pixel coords to logical coords used by the UI (origin top-left)
void pixelToLogical(int px, int py, int *outX, int *outY) {
    float lx = (float)(px - viewX) / (float)viewScale;
    float ly = (float)(py - viewY) / (float)viewScale;
    if (outX) *outX = (int)lx;
    if (outY) *outY = (int)ly;
}

// Reshape handler — keep content centered and scale to maintain aspect ratio
void reshape(int w, int h) {
    windowWidth = w; windowHeight = h;
    // compute scale to fit logical WINDOW_WIDTH x WINDOW_HEIGHT into window while preserving aspect
    float sx = (float)w / (float)WINDOW_WIDTH;
    float sy = (float)h / (float)WINDOW_HEIGHT;
    viewScale = (sx < sy) ? sx : sy;
    viewW = (int)(WINDOW_WIDTH * viewScale);
    viewH = (int)(WINDOW_HEIGHT * viewScale);
    viewX = (w - viewW) / 2;
    viewY = (h - viewH) / 2;
    // Set projection to match logical coords, mapped into viewport
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// Draw clouds
void drawCloud(float x, float y, float size) {
    glColor3f(1.0f, 1.0f, 1.0f);
    
    // Main cloud circles
    drawFilledCircle(x, y, size * 0.6f, 20);
    drawFilledCircle(x - size * 0.4f, y, size * 0.4f, 20);
    drawFilledCircle(x + size * 0.4f, y, size * 0.4f, 20);
    drawFilledCircle(x - size * 0.2f, y - size * 0.3f, size * 0.3f, 20);
    drawFilledCircle(x + size * 0.2f, y - size * 0.3f, size * 0.3f, 20);
    drawFilledCircle(x, y + size * 0.2f, size * 0.5f, 20);
}

// Draw all clouds
void drawClouds() {
    drawCloud(150, 80, 60);
    drawCloud(400, 60, 80);
    drawCloud(650, 90, 70);
    drawCloud(850, 70, 65);
}

// Draw animated waves
void drawWaves() {
    // Water background
    glColor3f(0.0f, 0.7f, 0.9f);
    glBegin(GL_QUADS);
    glVertex2f(0, 280);
    glVertex2f(WINDOW_WIDTH, 280);
    glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
    glVertex2f(0, WINDOW_HEIGHT);
    glEnd();
    
    // Wave layers - lighter blue
    glColor3f(0.2f, 0.8f, 1.0f);
    glBegin(GL_TRIANGLE_STRIP);
    for(int x = 0; x <= WINDOW_WIDTH; x += 10) {
        float wave1 = sin((x + waveOffset) * 0.01f) * 15;
        float wave2 = sin((x + waveOffset * 1.5f) * 0.008f) * 10;
        glVertex2f(x, 280 + wave1 + wave2);
        glVertex2f(x, 320 + wave1 + wave2);
    }
    glEnd();
    
    // Second wave layer - even lighter
    glColor3f(0.4f, 0.85f, 1.0f);
    glBegin(GL_TRIANGLE_STRIP);
    for(int x = 0; x <= WINDOW_WIDTH; x += 10) {
        float wave1 = sin((x + waveOffset * 0.8f) * 0.012f) * 12;
        float wave2 = sin((x + waveOffset * 1.2f) * 0.009f) * 8;
        glVertex2f(x, 300 + wave1 + wave2);
        glVertex2f(x, 340 + wave1 + wave2);
    }
    glEnd();
}

// Draw boat
void drawBoat() {
    float boatX = 580;
    float boatY = 270;
    
    // Boat shadow/depth
    glColor3f(0.4f, 0.2f, 0.1f);
    glBegin(GL_QUADS);
    glVertex2f(boatX - 55, boatY + 15);
    glVertex2f(boatX + 55, boatY + 15);
    glVertex2f(boatX + 45, boatY + 35);
    glVertex2f(boatX - 45, boatY + 35);
    glEnd();
    
    // Main boat body
    glColor3f(0.6f, 0.4f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(boatX - 50, boatY);
    glVertex2f(boatX + 50, boatY);
    glVertex2f(boatX + 40, boatY + 30);
    glVertex2f(boatX - 40, boatY + 30);
    glEnd();
    
    // Boat rim
    glColor3f(0.5f, 0.3f, 0.1f);
    glLineWidth(3.0f);
    glBegin(GL_LINE_STRIP);
    glVertex2f(boatX - 50, boatY);
    glVertex2f(boatX + 50, boatY);
    glVertex2f(boatX + 40, boatY + 30);
    glVertex2f(boatX - 40, boatY + 30);
    glVertex2f(boatX - 50, boatY);
    glEnd();
}

// Draw fisherman
void drawFisherman() {
    float fishX = 580;
    float fishY = 220;
    
    // Hat
    glColor3f(0.2f, 0.4f, 0.2f);
    drawFilledCircle(fishX, fishY - 10, 18, 16);
    
    // Hat brim
    glBegin(GL_QUADS);
    glVertex2f(fishX - 25, fishY - 5);
    glVertex2f(fishX + 25, fishY - 5);
    glVertex2f(fishX + 22, fishY + 2);
    glVertex2f(fishX - 22, fishY + 2);
    glEnd();
    
    // Head
    glColor3f(1.0f, 0.8f, 0.6f);
    drawFilledCircle(fishX, fishY + 5, 12, 16);
    
    // Body
    glColor3f(0.8f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(fishX - 15, fishY + 15);
    glVertex2f(fishX + 15, fishY + 15);
    glVertex2f(fishX + 12, fishY + 45);
    glVertex2f(fishX - 12, fishY + 45);
    glEnd();
    
    // Arms
    glColor3f(1.0f, 0.8f, 0.6f);
    // Left arm
    glBegin(GL_QUADS);
    glVertex2f(fishX - 15, fishY + 20);
    glVertex2f(fishX - 25, fishY + 35);
    glVertex2f(fishX - 22, fishY + 38);
    glVertex2f(fishX - 12, fishY + 23);
    glEnd();
    
    // Right arm holding fishing rod
    glBegin(GL_QUADS);
    glVertex2f(fishX + 12, fishY + 23);
    glVertex2f(fishX + 30, fishY + 25);
    glVertex2f(fishX + 32, fishY + 28);
    glVertex2f(fishX + 15, fishY + 26);
    glEnd();
    
    // Fishing rod
    glColor3f(0.4f, 0.2f, 0.1f);
    glLineWidth(4.0f);
    glBegin(GL_LINES);
    glVertex2f(fishX + 30, fishY + 25);
    glVertex2f(fishX + 80, fishY - 20);
    glEnd();
    
    // Fishing line
    glColor3f(0.0f, 0.0f, 0.0f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    glVertex2f(fishX + 80, fishY - 20);
    glVertex2f(fishX + 85, fishY + 60);
    glEnd();
    
    // Hook at end of line
    glColor3f(0.6f, 0.6f, 0.6f);
    drawFilledCircle(fishX + 85, fishY + 60, 2, 8);
}

// Draw sky gradient
void drawSky() {
    // Top part - lighter blue
    glBegin(GL_QUADS);
    glColor3f(0.6f, 0.8f, 1.0f);
    glVertex2f(0, 0);
    glVertex2f(WINDOW_WIDTH, 0);
    glColor3f(1.0f, 1.0f, 0.8f);
    glVertex2f(WINDOW_WIDTH, 280);
    glVertex2f(0, 280);
    glEnd();
}

// Draw a glossy, 3D-like circular play button with shadow and outlined triangle
void drawPlayButton() {
    float buttonX = WINDOW_WIDTH / 2.0f;
    float buttonY = WINDOW_HEIGHT - 80.0f;
    float buttonRadius = 55.0f * playBtnScale;
    int segments = 64;
    float alpha = playBtnAlpha;

    // Shadow
    glPushMatrix();
    glTranslatef(0, 6, 0);
    for (int i = 0; i < 3; ++i) {
        float a = (0.18f - i * 0.05f) * alpha;
        float r = buttonRadius + 6 + i * 3;
        glColor4f(0.0f, 0.0f, 0.0f, a);
        drawFilledCircle(buttonX, buttonY, r, segments);
    }
    glPopMatrix();

    // Main button: radial green gradient
    for (int i = (int)buttonRadius; i > 0; --i) {
        float t = (float)i / (buttonRadius > 0 ? buttonRadius : 1);
        float r = 0.18f * t + 0.2f * (1-t);
        float g = 0.7f * t + 1.0f * (1-t);
        float b = 0.18f * t + 0.2f * (1-t);
        glColor4f(r, g, b, alpha);
        drawFilledCircle(buttonX, buttonY, i, segments);
    }

    // Glossy highlight
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(1.0f, 1.0f, 1.0f, 0.22f * alpha);
    glVertex2f(buttonX, buttonY + buttonRadius * 0.45f);
    for (int i = 0; i <= segments / 2; ++i) {
        float angle = PI + (float)i / (segments / 2) * PI;
        float x = buttonX + cosf(angle) * buttonRadius * 0.85f;
        float y = buttonY + sinf(angle) * buttonRadius * 0.65f;
        glVertex2f(x, y);
    }
    glEnd();

    // Play triangle
    float triW = 34.0f * playBtnScale, triH = 38.0f * playBtnScale;
    float triX = buttonX - triW * 0.32f;
    float triY = buttonY - triH / 2.0f;
    // Outline
    glColor4f(0.08f, 0.18f, 0.08f, alpha);
    glBegin(GL_TRIANGLES);
    glVertex2f(triX - 2, triY - 2);
    glVertex2f(triX - 2, triY + triH + 2);
    glVertex2f(triX + triW + 2, buttonY + 2);
    glEnd();
    // Main triangle
    glColor4f(1.0f, 1.0f, 1.0f, alpha);
    glBegin(GL_TRIANGLES);
    glVertex2f(triX, triY);
    glVertex2f(triX, triY + triH);
    glVertex2f(triX + triW, buttonY);
    glEnd();
}

// Draw UI elements
void drawUI() {
    // No UI elements - clean start screen
}


void drawText(float x, float y, void *font, const char *string) {
    glRasterPos2f(x, y);
    for (int k = 0; string[k] != '\0'; k++) {
        glutBitmapCharacter(font, string[k]);
    }
}

// Local PI helpers (avoid non-standard M_PI macros)
static const float LOCAL_PI = 3.14159265359f;
static const float LOCAL_PI_2 = 1.57079632679f;

// Draw a rounded rectangle (filled)
void drawRoundedRect(float x, float y, float w, float h, float r) {
    // center rectangle
    glBegin(GL_QUADS);
    glVertex2f(x + r, y);
    glVertex2f(x + w - r, y);
    glVertex2f(x + w - r, y + h);
    glVertex2f(x + r, y + h);
    glEnd();
    // left and right rects
    glBegin(GL_QUADS);
    glVertex2f(x, y + r);
    glVertex2f(x + r, y + r);
    glVertex2f(x + r, y + h - r);
    glVertex2f(x, y + h - r);
    glVertex2f(x + w - r, y + r);
    glVertex2f(x + w, y + r);
    glVertex2f(x + w, y + h - r);
    glVertex2f(x + w - r, y + h - r);
    glEnd();
    // corners as triangle fans
    const int seg = 12;
    // top-left
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x + r, y + r);
    for (int i = 0; i <= seg; ++i) {
        float a = LOCAL_PI + (LOCAL_PI_2 * i / seg);
        glVertex2f(x + r + cosf(a) * r, y + r + sinf(a) * r);
    }
    glEnd();
    // top-right
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x + w - r, y + r);
    for (int i = 0; i <= seg; ++i) {
        float a = -LOCAL_PI_2 + (LOCAL_PI_2 * i / seg);
        glVertex2f(x + w - r + cosf(a) * r, y + r + sinf(a) * r);
    }
    glEnd();
    // bottom-right
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x + w - r, y + h - r);
    for (int i = 0; i <= seg; ++i) {
        float a = 0 + (LOCAL_PI_2 * i / seg);
        glVertex2f(x + w - r + cosf(a) * r, y + h - r + sinf(a) * r);
    }
    glEnd();
    // bottom-left
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x + r, y + h - r);
    for (int i = 0; i <= seg; ++i) {
        float a = LOCAL_PI_2 + (LOCAL_PI_2 * i / seg);
        glVertex2f(x + r + cosf(a) * r, y + h - r + sinf(a) * r);
    }
    glEnd();
}

// Passive mouse motion for intro hover
void passiveMouse(int x, int y) {
    // convert to logical coordinates
    int lx, ly; pixelToLogical(x, y, &lx, &ly);
    introMouseX = lx; introMouseY = ly;
}

void drawIntroScreen() {
    glClearColor(0.06f, 0.35f, 0.65f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Title with shadow + gradient-ish fill (approx)
    float titleX = WINDOW_WIDTH / 2.0f;
    float titleY = 70.0f;
    // shadow
    glColor3f(0.05f, 0.05f, 0.05f);
    glRasterPos2f(titleX - 150 + 4, titleY + 4);
    for (const char *s = "Virtual Fishing"; *s; ++s) glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *s);
    // main (draw twice with different colors to mimic gradient)
    glColor3f(1.0f, 0.85f, 0.02f);
    glRasterPos2f(titleX - 150, titleY);
    for (const char *s = "Virtual Fishing"; *s; ++s) glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *s);

    // Team card
    float cardW = 540, cardH = 180;
    float cardX = (WINDOW_WIDTH - cardW)/2.0f;
    float cardY = 140;
    glColor4f(1.0f, 1.0f, 1.0f, 0.12f);
    drawRoundedRect(cardX, cardY, cardW, cardH, 12.0f);
    // Team heading (larger)
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(cardX + 18, cardY + 30);
    for (const char *s = "Team Members:"; *s; ++s) glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *s);
    // Names
    const char* names[] = {"1. Prajna                NNM22CS126","2. Varsha               NNM22CS199","3. Shifali Mada       NNM22CS166","4. Rithika Rai          NNM22CS144","5. Devanmitha         NNM22CS059"};
    for (int i = 0; i < 5; ++i) {
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2f(cardX + 28, cardY + 58 + i * 26);
        for (const char *p = names[i]; *p; ++p) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *p);
    }

    // START button (modern gradient look) - static (no pulsing)
    float btnW = 220, btnH = 60;
    float btnX = (WINDOW_WIDTH - btnW)/2.0f;
    float btnY = cardY + cardH + 26;
    // draw rounded green gradient approx by two rects (no animation)
    glColor3f(0.06f, 0.45f, 0.12f);
    drawRoundedRect(btnX, btnY, btnW, btnH, 10.0f);
    glColor4f(0.22f, 0.82f, 0.36f, 0.95f);
    drawRoundedRect(btnX + 4, btnY + 4, btnW - 8, btnH - 8, 8.0f);
    // Button text (larger)
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(btnX + btnW/2 - 32, btnY + btnH/2 + 8);
    for (const char *s = "START"; *s; ++s) glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *s);

    glutSwapBuffers();
}

void drawFishermanScreen() {
    glClear(GL_COLOR_BUFFER_BIT);
    drawSky();
    drawClouds();
    drawWaves();
    drawBoat();
    drawFisherman();
    // Fade out fisherman/boat as game fades in
    if (gameAlpha < 1.0f)
        drawPlayButton();
    glutSwapBuffers();
}

void drawUnderwaterPlants() {
    glPushMatrix();
    glTranslatef(0, WINDOW_HEIGHT, 0);
    for (int i = 0; i < 12; ++i) {
        float x = 60 + i * 80 + 20 * sinf(i);
        glColor4f(0.1f, 0.5f + 0.2f * (i%2), 0.2f, 0.18f + 0.12f * (i%2));
        glBegin(GL_LINE_STRIP);
        for (int j = 0; j < 18; ++j) {
            float y = -j * 18;
            float dx = 8 * sinf(j * 0.5f + i);
            glVertex2f(x + dx, y);
        }
        glEnd();
    }
    glPopMatrix();
}

void drawGameFish() {
    for (int i = 0; i < GAME_FISH_COUNT; ++i) {
        GameFish *f = &gameFish[i];
        if (!f->active) continue; // Skip inactive fish
        float dir = (f->speed > 0) ? 1.0f : -1.0f;
        float bodyX = f->x;
        float bodyY = f->y;
    float rx = 32, ry = 18; // Increased fish body size
        // Body (ellipse as polygon)
        glColor4f(f->r, f->g, f->b, gameAlpha);
        glBegin(GL_POLYGON);
        for (int j = 0; j < 32; ++j) {
            float a = 2 * PI * j / 32;
            float px = bodyX + dir * cosf(a) * rx;
            float py = bodyY + sinf(a) * ry;
            glVertex2f(px, py);
        }
        glEnd();
        // Tail (increased size)
        glColor4f(f->r * 0.7f, f->g * 0.7f, f->b * 0.7f, gameAlpha);
        glBegin(GL_TRIANGLES);
        if (dir > 0) {
            glVertex2f(bodyX - 26, bodyY);
            glVertex2f(bodyX - 46, bodyY - 16);
            glVertex2f(bodyX - 46, bodyY + 16);
        } else {
            glVertex2f(bodyX + 26, bodyY);
            glVertex2f(bodyX + 46, bodyY - 16);
            glVertex2f(bodyX + 46, bodyY + 16);
        }
        glEnd();
        // Eye
        glColor4f(1, 1, 1, gameAlpha);
        if (dir > 0) {
            drawFilledCircle(bodyX + 10, bodyY - 3, 3, 8);
            glColor4f(0, 0, 0, gameAlpha);
            drawFilledCircle(bodyX + 12, bodyY - 3, 1.2f, 8);
        } else {
            drawFilledCircle(bodyX - 10, bodyY - 3, 3, 8);
            glColor4f(0, 0, 0, gameAlpha);
            drawFilledCircle(bodyX - 12, bodyY - 3, 1.2f, 8);
        }
    }
}

// Draw fishing line and hook (mouse controlled)
void drawFishingLineAndHook() {
    float cx = WINDOW_WIDTH / 2.0f;
    float topY = 0.0f;
    float loopR = 6.0f;
    float hookTopX = cx;
    float hookTopY = topY;
    float hookEndX = hookX;
    float hookEndY = hookY;

    // Draw the rope (line from top center to hook)
    glColor4f(0, 0, 0, gameAlpha);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2f(hookTopX, hookTopY);
    glVertex2f(hookEndX, hookEndY);
    glEnd();

    // Draw the loop at the hook
    glColor4f(0.7f, 0.7f, 0.7f, gameAlpha);
    glLineWidth(2.5f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 20; ++i) {
        float a = 2 * PI * i / 20.0f;
        glVertex2f(hookEndX + cosf(a) * loopR, hookEndY + sinf(a) * loopR);
    }
    glEnd();

    // Draw a simple static worm hanging from the hook.
    // The worm is a slightly curved, thick ribbon (S-like) built with a GL_QUAD_STRIP
    // so it appears rounded and natural. It uses hookEndX/hookEndY so it follows the hook.
    {
        // control points (relative offsets from hook tip) describing an S-curve
        float pts[5][2] = {
            {0.0f, 8.0f},   // near the loop
            {6.0f, 22.0f},  // curve right
            {-5.0f, 36.0f}, // curve left
            {7.0f, 50.0f},  // right again
            {0.0f, 64.0f}   // tail
        };
        float halfWidth = 5.5f; // thickness of the worm
        float wormR = 0.72f, wormG = 0.38f, wormB = 0.26f; // light brown/pinkish tone

        glColor4f(wormR, wormG, wormB, 1.0f * gameAlpha);
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i < 5; ++i) {
            float cx = hookEndX + pts[i][0];
            float cy = hookEndY + pts[i][1];

            // compute a perpendicular (normal) based on segment direction
            float nx = 0.0f, ny = -1.0f;
            if (i < 4) {
                float tx = (hookEndX + pts[i+1][0]) - cx;
                float ty = (hookEndY + pts[i+1][1]) - cy;
                float len = sqrtf(tx*tx + ty*ty);
                if (len > 1e-4f) { nx = -ty / len; ny = tx / len; }
            } else {
                float tx = cx - (hookEndX + pts[i-1][0]);
                float ty = cy - (hookEndY + pts[i-1][1]);
                float len = sqrtf(tx*tx + ty*ty);
                if (len > 1e-4f) { nx = -ty / len; ny = tx / len; }
            }

            float lx = cx + nx * halfWidth;
            float ly = cy + ny * halfWidth;
            float rx = cx - nx * halfWidth;
            float ry = cy - ny * halfWidth;

            glVertex2f(lx, ly);
            glVertex2f(rx, ry);
        }
        glEnd();

        // subtle highlight down the worm's upper side to suggest volume
        glColor4f(1.0f, 0.85f, 0.6f, 0.14f * gameAlpha);
        glLineWidth(2.0f);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i < 5; ++i) {
            float cx = hookEndX + pts[i][0] - 2.0f; // slight offset for highlight
            float cy = hookEndY + pts[i][1] - 2.0f;
            glVertex2f(cx, cy);
        }
        glEnd();
    }

    // Draw smooth curved hook (BLACK)
    glColor4f(0.0f, 0.0f, 0.0f, gameAlpha);
    glLineWidth(3.0f);
    glBegin(GL_LINE_STRIP);
    // Straight shank
    glVertex2f(hookEndX, hookEndY);
    glVertex2f(hookEndX, hookEndY + 25);
    // Smooth curve using arc (like a "J")
    float startY = hookEndY + 25;
    float radius = 18.0f;
    float centerX = hookEndX + radius;
    float centerY = startY;
    for (int i = 0; i <= 25; ++i) {
        float angle = PI - (PI * i / 25.0f);
        float x = centerX + cosf(angle) * radius;
        float y = centerY + sinf(angle) * radius;
        glVertex2f(x, y);
    }
    glVertex2f(centerX + radius, centerY - 5);
    glVertex2f(centerX + radius - 3, centerY - 15);
    glVertex2f(centerX + radius - 8, centerY - 22);
    glEnd();
    // Draw the barb (BLACK)
    glBegin(GL_LINES);
    glVertex2f(centerX + radius - 8, centerY - 22);
    glVertex2f(centerX + radius - 15, centerY - 20);
    glEnd();
}

void drawScore() {
    char buf[32];
    sprintf(buf, "Score: %d", score);
    glColor3f(1, 1, 0);
    glRasterPos2f(20, 30);
    for (int k = 0; buf[k] != '\0'; k++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, buf[k]);
    }
}
void checkHookFishCollision(void) {
    if (!hookControlActive) return; // Only check during upward scroll
    float hookRadius = 10.0f;
    for (int i = 0; i < GAME_FISH_COUNT; ++i) {
        GameFish *f = &gameFish[i];
        if (!f->active) continue;
        float dx = hookX - f->x;
        float dy = hookY - f->y;
        float distSq = dx*dx + dy*dy;
        float fishRadius = 22.0f;
        if (distSq < (hookRadius + fishRadius)*(hookRadius + fishRadius)) {
            f->active = 0; // Remove fish
            score += 10;
            fishRespawnTimers[i] = FISH_RESPAWN_DELAY; // Start respawn timer
        }
    }
}
void updateFishRespawnTimers(int elapsedMs) {
    for (int i = 0; i < GAME_FISH_COUNT; ++i) {
        if (!gameFish[i].active && fishRespawnTimers[i] > 0) {
            fishRespawnTimers[i] -= elapsedMs;
            if (fishRespawnTimers[i] <= 0) {
                // Respawn new fish at left or right edge
                float colors[6][3] = {{0.2f,0.9f,0.2f},{0.7f,0.2f,0.8f},{0.2f,0.7f,0.9f},{0.9f,0.7f,0.2f},{0.9f,0.2f,0.4f},{0.2f,0.2f,0.9f}};
                int c = rand() % 6;
                int dir = (rand() % 2 == 0) ? 1 : -1; // 1 = right, -1 = left
                float speed = dir * (1.2f + 0.7f * (rand()%100)/100.0f);
                float x = (dir > 0) ? -40.0f : (WINDOW_WIDTH + 40.0f);
                float y = 350 + rand() % 180;
                gameFish[i].x = x;
                gameFish[i].y = y;
                gameFish[i].r = colors[c][0];
                gameFish[i].g = colors[c][1];
                gameFish[i].b = colors[c][2];
                gameFish[i].speed = speed;
                gameFish[i].active = 1;
                fishRespawnTimers[i] = 0;
            }
        }
    }
}

// Reset game state: fish, score, scrolling timers
void resetGameState() {
    // Reset fish
    for (int i = 0; i < GAME_FISH_COUNT; ++i) {
        gameFish[i].x = rand() % (WINDOW_WIDTH - 80) + 40;
        gameFish[i].y = 350 + rand() % 180;
        float colors[6][3] = {{0.2f,0.9f,0.2f},{0.7f,0.2f,0.8f},{0.2f,0.7f,0.9f},{0.9f,0.7f,0.2f},{0.9f,0.2f,0.4f},{0.2f,0.2f,0.9f}};
        int c = rand() % 6;
        gameFish[i].r = colors[c][0];
        gameFish[i].g = colors[c][1];
        gameFish[i].b = colors[c][2];
        gameFish[i].speed = (rand()%2==0?1:-1) * (1.2f + 0.7f * (rand()%100)/100.0f);
        gameFish[i].active = 1;
        fishRespawnTimers[i] = 0;
    }
    // Reset score and background scrolling
    score = 0;
    bgScrollOffset = 0.0f;
    bgScrollDirection = 1;
    bgScrollTimer = 0;
    bgScrollActive = 1;
    // Restart scrolling timer and run one update immediately
    glutTimerFunc(18, updateScroll, 0);
    updateScroll(0);
    hookControlActive = 0;
}
// 5x7 pixel font for digits 0-9 (each digit is 5 columns, 7 rows; LSB=top)
static const unsigned char digitFont[10][5] = {
    {0x7F,0x41,0x41,0x41,0x7F}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x62,0x51,0x49,0x49,0x46}, // 2
    {0x22,0x41,0x49,0x49,0x36}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x2F,0x49,0x49,0x49,0x31}, // 5
    {0x3E,0x49,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}, // 9
};
// Draw a retro pixel digit at x,y with scale and color
void drawPixelDigit(int digit, float x, float y, float scale, float r, float g, float b, float alpha) {
    if (digit < 0 || digit > 9) return;
    glColor4f(r, g, b, alpha);
    for (int col = 0; col < 5; ++col) {
        unsigned char colbits = digitFont[digit][col];
        for (int row = 0; row < 7; ++row) {
            if (colbits & (1 << row)) {
                float px = x + col * scale;
                float py = y + row * scale;
                glBegin(GL_QUADS);
                glVertex2f(px, py);
                glVertex2f(px + scale, py);
                glVertex2f(px + scale, py + scale);
                glVertex2f(px, py + scale);
                glEnd();
            }
        }
    }
}
// Draw centered popup with score and buttons
void drawPopup() {
    if (!popupActive) return;
    // Dimmed overlay
    glColor4f(0,0,0,0.5f * popupAlpha);
    glBegin(GL_QUADS);
    glVertex2f(0,0); glVertex2f(WINDOW_WIDTH,0);
    glVertex2f(WINDOW_WIDTH,WINDOW_HEIGHT); glVertex2f(0,WINDOW_HEIGHT);
    glEnd();

    // Popup box
    float boxW = 560, boxH = 280;
    float bx = (WINDOW_WIDTH - boxW) / 2.0f;
    float by = (WINDOW_HEIGHT - boxH) / 2.0f;
    // Background
    glColor4f(0.95f, 0.95f, 0.92f, 0.98f * popupAlpha);
    glBegin(GL_QUADS);
    glVertex2f(bx, by); glVertex2f(bx + boxW, by);
    glVertex2f(bx + boxW, by + boxH); glVertex2f(bx, by + boxH);
    glEnd();
    // Border
    glColor4f(0.1f,0.1f,0.1f, popupAlpha);
    glLineWidth(3.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(bx, by); glVertex2f(bx + boxW, by);
    glVertex2f(bx + boxW, by + boxH); glVertex2f(bx, by + boxH);
    glEnd();

    // Title text
    glColor4f(0.05f,0.05f,0.2f, popupAlpha);
    char title[] = "Final Score";
    glRasterPos2f(bx + boxW/2 - 40, by + 34);
    for (int i = 0; title[i]; ++i) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, title[i]);

    // Draw score in retro pixel font, large scale
    char buf[32];
    sprintf(buf, "%d", score);
    float digitScale = 14.0f; // each pixel block
    float totalDigitsW = strlen(buf) * 5 * digitScale + (strlen(buf)-1) * digitScale;
    float startX = bx + (boxW - totalDigitsW)/2.0f;
    float startY = by + boxH/2 - (7*digitScale)/2;
    for (size_t i = 0; i < strlen(buf); ++i) {
        int d = buf[i] - '0';
        drawPixelDigit(d, startX + i * 6 * digitScale, startY, digitScale, 0.02f, 0.2f, 0.6f, popupAlpha);
    }

    // Buttons
    float btnW = 180, btnH = 46;
    float gap = 40;
    float btnY = by + boxH - 70;
    float btn1X = bx + boxW/2 - btnW - gap/2;
    float btn2X = bx + boxW/2 + gap/2;

    // Go to Home button
    glColor4f(0.2f,0.6f,0.2f, popupAlpha);
    glBegin(GL_QUADS);
    glVertex2f(btn1X, btnY); glVertex2f(btn1X + btnW, btnY);
    glVertex2f(btn1X + btnW, btnY + btnH); glVertex2f(btn1X, btnY + btnH);
    glEnd();
    glColor4f(1,1,1,popupAlpha);
    glRasterPos2f(btn1X + 36, btnY + 30);
    char homeLabel[] = "Go to Home";
    for (int i = 0; homeLabel[i]; ++i) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, homeLabel[i]);

    // Play Again button
    glColor4f(0.2f,0.4f,0.8f, popupAlpha);
    glBegin(GL_QUADS);
    glVertex2f(btn2X, btnY); glVertex2f(btn2X + btnW, btnY);
    glVertex2f(btn2X + btnW, btnY + btnH); glVertex2f(btn2X, btnY + btnH);
    glEnd();
    glColor4f(1,1,1,popupAlpha);
    glRasterPos2f(btn2X + 38, btnY + 30);
    char playLabel[] = "Play Again";
    for (int i = 0; playLabel[i]; ++i) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, playLabel[i]);
}

void drawGameScreen() {
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Draw background image (with scrolling)
    drawBackground();
    
    // ...waves and underwater plants removed...
    drawFishingLineAndHook();
    checkHookFishCollision();
    drawGameFish();
    drawScore();
    // Fade in
    if (gameAlpha < 1.0f) {
        glColor4f(0,0,0,1.0f-gameAlpha);
        glBegin(GL_QUADS);
        glVertex2f(0,0); glVertex2f(WINDOW_WIDTH,0);
        glVertex2f(WINDOW_WIDTH,WINDOW_HEIGHT); glVertex2f(0,WINDOW_HEIGHT);
        glEnd();
    }
    // Draw popup overlay if active
    drawPopup();
    glutSwapBuffers();
}








// Update function for scrolling and timing
void updateScroll(int value) {
    if (bgScrollActive) {
        // 18ms per frame (about 55 FPS)
        bgScrollTimer += 18;
        // 0..DOWN_DURATION_MS: scroll down, next UP_DURATION_MS: scroll up, then stop
        if (bgScrollTimer < DOWN_DURATION_MS) {
            bgScrollDirection = 1;
            hookControlActive = 0;
        } else if (bgScrollTimer < DOWN_DURATION_MS + UP_DURATION_MS) {
            bgScrollDirection = -1;
            hookControlActive = 1;
        } else {
            bgScrollDirection = 0;
            bgScrollActive = 0;
            hookControlActive = 0;
            // Reset hook position to initial
            hookX = WINDOW_WIDTH / 2.0f;
            hookY = 320.0f;
            // Show final score popup with fade-in
            popupActive = 1;
            popupAnimating = 1;
            popupAlpha = 0.0f;
        }
        // Increase scroll speed during upward phase
        float scrollSpeed = bgScrollSpeed;
        if (bgScrollDirection == -1) {
            scrollSpeed = bgScrollSpeed * UP_SPEED_FACTOR; // faster when scrolling up
        }
        bgScrollOffset += bgScrollDirection * scrollSpeed;
        // Loop offset to keep seamless
        if (bgHeight > 0) {
            if (bgScrollOffset >= bgHeight) bgScrollOffset -= bgHeight;
            if (bgScrollOffset < 0) bgScrollOffset += bgHeight;
        }
        glutPostRedisplay();
        glutTimerFunc(18, updateScroll, 0);
    }
}

void display() {
    // Set viewport to centered logical area
    glViewport(viewX, viewY, viewW, viewH);
    glScissor(viewX, viewY, viewW, viewH);
    glEnable(GL_SCISSOR_TEST);
    // Drawing occurs in logical coordinate space (0..WINDOW_WIDTH, 0..WINDOW_HEIGHT)
    if (screen == 0)
        drawIntroScreen();
    else if (screen == 1)
        drawFishermanScreen();
    else if (screen == 2)
        drawGameScreen();
    // restore full-window viewport for any overlays
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, windowWidth, windowHeight);
}

// Mouse click handler
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        // convert pixel coords to logical coords used by drawing (so clicks map correctly when fullscreen/resized)
        int lx, ly; pixelToLogical(x, y, &lx, &ly);
        if (screen == 0) {
            // Use same START button geometry as drawIntroScreen()
            float btnW = 220, btnH = 60;
            float cardW = 540, cardH = 180;
            float cardX = (WINDOW_WIDTH - cardW)/2.0f;
            float cardY = 140;
            float btnX = (WINDOW_WIDTH - btnW)/2.0f;
            float btnY = cardY + cardH + 26;
            int top = (int)btnY;
            int bottom = (int)(btnY + btnH);
            int left = (int)btnX;
            int right = (int)(btnX + btnW);
            if (lx >= left && lx <= right && ly >= top && ly <= bottom) {
                screen = 1;
                glutPostRedisplay();
            }
        } else if (screen == 1 && !animatingTransition && gameAlpha < 0.01f) {
            // Play button click (use logical coords)
            float buttonX = WINDOW_WIDTH / 2.0f;
            float buttonY = WINDOW_HEIGHT - 80.0f;
            float dx = lx - buttonX, dy = ly - buttonY;
            if (dx*dx + dy*dy <= 55*55) {
                resetGameState();
                animatingTransition = 1;
                playBtnAnim = 0.0f;
            }
        } else if (screen == 2 && popupActive) {
            // If popup active, check buttons (logical coords)
            float boxW = 560, boxH = 280;
            float bx = (WINDOW_WIDTH - boxW) / 2.0f;
            float by = (WINDOW_HEIGHT - boxH) / 2.0f;
            float btnW = 180, btnH = 46;
            float gap = 40;
            float btnY = by + boxH - 70;
            float btn1X = bx + boxW/2 - btnW - gap/2;
            float btn2X = bx + boxW/2 + gap/2;
            // Check Go to Home
            if (lx >= (int)btn1X && lx <= (int)(btn1X+btnW) && ly >= (int)btnY && ly <= (int)(btnY+btnH)) {
                popupActive = 0;
                popupAnimating = 0;
                popupAlpha = 0.0f;
                screen = 1; // fisherman screen
                gameAlpha = 0.0f;
                playBtnAlpha = 1.0f;
                playBtnScale = 1.0f;
                animatingTransition = 0;
                glutPostRedisplay();
            }
            // Check Play Again
            if (lx >= (int)btn2X && lx <= (int)(btn2X+btnW) && ly >= (int)btnY && ly <= (int)(btnY+btnH)) {
                popupActive = 0;
                popupAnimating = 0;
                popupAlpha = 0.0f;
                for (int i = 0; i < GAME_FISH_COUNT; ++i) {
                    gameFish[i].x = rand() % (WINDOW_WIDTH - 80) + 40;
                    gameFish[i].y = 350 + rand() % 180;
                    int c = rand() % 6;
                    float colors[6][3] = {{0.2f,0.9f,0.2f},{0.7f,0.2f,0.8f},{0.2f,0.7f,0.9f},{0.9f,0.7f,0.2f},{0.9f,0.2f,0.4f},{0.2f,0.2f,0.9f}};
                    gameFish[i].r = colors[c][0];
                    gameFish[i].g = colors[c][1];
                    gameFish[i].b = colors[c][2];
                    gameFish[i].speed = (rand()%2==0?1:-1) * (1.2f + 0.7f * (rand()%100)/100.0f);
                    gameFish[i].active = 1;
                    fishRespawnTimers[i] = 0;
                }
                score = 0;
                bgScrollOffset = 0.0f;
                bgScrollDirection = 1;
                bgScrollTimer = 0;
                bgScrollActive = 1;
                glutTimerFunc(18, updateScroll, 0);
                updateScroll(0);
                hookControlActive = 0;
                glutPostRedisplay();
            }
        }
    }
}

// Mouse motion handler for hook control
void mouseMotion(int x, int y) {
    if (hookControlActive) {
        int lx, ly; pixelToLogical(x, y, &lx, &ly);
        hookX = lx;
        hookY = ly;
        glutPostRedisplay();
    }
}

// Animation timer
void timer(int value) {
    int needRedisplay = 0;
    int elapsedMs = 18;
    if (screen == 1) {
        waveOffset += 2.0f;
        if(waveOffset > 628) waveOffset = 0;
        needRedisplay = 1;
        // Animate play button transition
        if (animatingTransition) {
            playBtnAnim += 0.06f;
            if (playBtnAnim > 1.0f) playBtnAnim = 1.0f;
            playBtnScale = 1.0f - 0.18f * playBtnAnim;
            playBtnAlpha = 1.0f - playBtnAnim;
            gameAlpha = playBtnAnim;
            if (playBtnAnim >= 1.0f) {
                animatingTransition = 0;
                playBtnScale = 1.0f;
                playBtnAlpha = 1.0f;
                screen = 2;
                gameAlpha = 1.0f;
            }
            needRedisplay = 1;
        }
    } else if (screen == 2) {
        // Move fish
        if (!popupActive) {
            for (int i = 0; i < GAME_FISH_COUNT; ++i) {
                if (gameFish[i].active) {
                    gameFish[i].x += gameFish[i].speed;
                    if (gameFish[i].speed > 0 && gameFish[i].x > WINDOW_WIDTH + 40)
                        gameFish[i].x = -40;
                    if (gameFish[i].speed < 0 && gameFish[i].x < -40)
                        gameFish[i].x = WINDOW_WIDTH + 40;
                }
            }
        }
        updateFishRespawnTimers(elapsedMs);
        needRedisplay = 1;
    }
    // Advance popup fade-in animation
    if (popupAnimating) {
        popupAlpha += 0.04f; // fade speed per frame
        if (popupAlpha >= 1.0f) {
            popupAlpha = 1.0f;
            popupAnimating = 0;
        }
        needRedisplay = 1;
    }
    if (needRedisplay)
        glutPostRedisplay();
    glutTimerFunc(18, timer, 0);
}

// Initialize OpenGL
// Load and setup background texture
void loadBackgroundTexture() {
    // Load image using stb_image
    backgroundImage = stbi_load("background.png", &bgWidth, &bgHeight, &bgChannels, 4);
    if (!backgroundImage) {
        printf("Failed to load background image\n");
        return;
    }

    // Generate and bind OpenGL texture
    glGenTextures(1, &backgroundTexture);
    glBindTexture(GL_TEXTURE_2D, backgroundTexture);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bgWidth, bgHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, backgroundImage);

    // Free the image data as it's now in GPU memory
    stbi_image_free(backgroundImage);
    backgroundImage = NULL;
}

// Draw the background texture
void drawBackground() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, backgroundTexture);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    // Calculate vertical offset in texture coordinates
    float texOffset = 0.0f;
    if (bgHeight > 0) {
        texOffset = fmodf(bgScrollOffset, (float)bgHeight);
        if (texOffset < 0) texOffset += bgHeight;
        texOffset /= (float)bgHeight;
    }

    // Draw two quads to seamlessly loop the image vertically
    glBegin(GL_QUADS);
    // First (main) image
    glTexCoord2f(0.0f, texOffset); glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, texOffset); glVertex2f(WINDOW_WIDTH, 0.0f);
    glTexCoord2f(1.0f, texOffset + (float)WINDOW_HEIGHT/(float)bgHeight); glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
    glTexCoord2f(0.0f, texOffset + (float)WINDOW_HEIGHT/(float)bgHeight); glVertex2f(0.0f, WINDOW_HEIGHT);
    glEnd();

    // If the offset means part of the image is off the bottom, draw the top part again
    if (texOffset + (float)WINDOW_HEIGHT/(float)bgHeight > 1.0f) {
        float over = texOffset + (float)WINDOW_HEIGHT/(float)bgHeight - 1.0f;
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, WINDOW_HEIGHT - over * bgHeight);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT - over * bgHeight);
        glTexCoord2f(1.0f, over); glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
        glTexCoord2f(0.0f, over); glVertex2f(0.0f, WINDOW_HEIGHT);
        glEnd();
    }
    glDisable(GL_TEXTURE_2D);
}

void init() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    // Set up 2D orthographic projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    // Load background texture
    loadBackgroundTexture();
    // Start scrolling timer
    bgScrollOffset = 0.0f;
    bgScrollDirection = 1;
    bgScrollTimer = 0;
    bgScrollActive = 1;
    glutTimerFunc(18, updateScroll, 0);
    glutMotionFunc(mouseMotion);
    glutPassiveMotionFunc(passiveMouse);
}

void cleanup() {
    // Delete texture when program exits
    if (glIsTexture(backgroundTexture)) {
        glDeleteTextures(1, &backgroundTexture);
    }
    // Free image data if it wasn't freed
    if (backgroundImage) {
        stbi_image_free(backgroundImage);
        backgroundImage = NULL;
    }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Fishing Game");
    
    // Start in fullscreen mode
    glutFullScreen();
    // Ensure reshape handler is registered so viewport/projection update when resolution changes
    glutReshapeFunc(reshape);
    // Register cleanup function to be called on program exit
    atexit(cleanup);

    init();

    // Initialize game fish
    srand((unsigned)time(NULL));
    for (int i = 0; i < GAME_FISH_COUNT; ++i) {
        gameFish[i].x = rand() % (WINDOW_WIDTH - 80) + 40;
        gameFish[i].y = 350 + rand() % 180;
        float colors[6][3] = {{0.2f,0.9f,0.2f},{0.7f,0.2f,0.8f},{0.2f,0.7f,0.9f},{0.9f,0.7f,0.2f},{0.9f,0.2f,0.4f},{0.2f,0.2f,0.9f}};
        int c = rand() % 6;
        gameFish[i].r = colors[c][0];
        gameFish[i].g = colors[c][1];
        gameFish[i].b = colors[c][2];
        gameFish[i].speed = (rand()%2==0?1:-1) * (1.2f + 0.7f * (rand()%100)/100.0f);
        gameFish[i].active = 1;
        fishRespawnTimers[i] = 0;
    }

    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutTimerFunc(0, timer, 0);

    glutMainLoop();
    return 0;
// End of drawFishingLineAndHook
}

