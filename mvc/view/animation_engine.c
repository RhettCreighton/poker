#include "animation_engine.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Easing function implementations
float ease_linear(float t) {
    return t;
}

float ease_in_quad(float t) {
    return t * t;
}

float ease_out_quad(float t) {
    return t * (2 - t);
}

float ease_in_out_quad(float t) {
    return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t;
}

float ease_in_cubic(float t) {
    return t * t * t;
}

float ease_out_cubic(float t) {
    return 1 + (--t) * t * t;
}

float ease_in_out_cubic(float t) {
    return t < 0.5 ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
}

float ease_in_elastic(float t) {
    if (t == 0 || t == 1) return t;
    float p = 0.3;
    return -pow(2, 10 * (t - 1)) * sin((t - 1.1) * 2 * M_PI / p);
}

float ease_out_elastic(float t) {
    if (t == 0 || t == 1) return t;
    float p = 0.3;
    return pow(2, -10 * t) * sin((t - 0.1) * 2 * M_PI / p) + 1;
}

float ease_out_bounce(float t) {
    if (t < 1/2.75) {
        return 7.5625 * t * t;
    } else if (t < 2/2.75) {
        t -= 1.5/2.75;
        return 7.5625 * t * t + 0.75;
    } else if (t < 2.5/2.75) {
        t -= 2.25/2.75;
        return 7.5625 * t * t + 0.9375;
    } else {
        t -= 2.625/2.75;
        return 7.5625 * t * t + 0.984375;
    }
}

float ease_out_back(float t) {
    float c1 = 1.70158;
    float c3 = c1 + 1;
    return 1 + c3 * pow(t - 1, 3) + c1 * pow(t - 1, 2);
}

// Engine management
AnimationEngine* anim_engine_create(void) {
    AnimationEngine* engine = calloc(1, sizeof(AnimationEngine));
    if (!engine) return NULL;
    
    engine->max_particles = 1000;
    engine->time_scale = 1.0f;
    
    return engine;
}

void anim_engine_destroy(AnimationEngine* engine) {
    if (!engine) return;
    
    // Free particles
    Particle* p = engine->particles;
    while (p) {
        Particle* next = p->next;
        free(p);
        p = next;
    }
    
    free(engine);
}

void anim_engine_update(AnimationEngine* engine) {
    if (!engine) return;
    
    engine->frame_count++;
    
    // Update particles
    anim_particle_update(engine);
    
    // Update screen shake
    if (engine->shake_duration > 0) {
        engine->shake_counter++;
        engine->shake_duration--;
        if (engine->shake_duration == 0) {
            engine->shake_intensity = 0;
        }
    }
}

// Transform animations
void anim_transform_init(AnimationTransform* transform, float start_x, float start_y, 
                        float end_x, float end_y, float speed) {
    transform->start_x = start_x;
    transform->start_y = start_y;
    transform->end_x = end_x;
    transform->end_y = end_y;
    transform->x = start_x;
    transform->y = start_y;
    transform->progress = 0.0f;
    transform->speed = speed;
}

bool anim_transform_update(AnimationTransform* transform, float (*easing_func)(float)) {
    if (transform->progress >= 1.0f) return true;
    
    transform->progress += transform->speed;
    if (transform->progress > 1.0f) transform->progress = 1.0f;
    
    float eased = easing_func ? easing_func(transform->progress) : transform->progress;
    
    transform->x = transform->start_x + (transform->end_x - transform->start_x) * eased;
    transform->y = transform->start_y + (transform->end_y - transform->start_y) * eased;
    
    return transform->progress >= 1.0f;
}

void anim_transform_get_position(AnimationTransform* transform, int* x, int* y) {
    *x = (int)round(transform->x);
    *y = (int)round(transform->y);
}

// Particle effects
void anim_particle_spawn(AnimationEngine* engine, float x, float y, 
                        float vx, float vy, uint32_t color, char symbol) {
    if (!engine) return;
    
    // Count current particles
    int count = 0;
    Particle* p = engine->particles;
    while (p) {
        count++;
        p = p->next;
    }
    
    if (count >= engine->max_particles) return;
    
    // Create new particle
    Particle* particle = calloc(1, sizeof(Particle));
    if (!particle) return;
    
    particle->x = x;
    particle->y = y;
    particle->vx = vx;
    particle->vy = vy;
    particle->life = 1.0f;
    particle->color = color;
    particle->symbol = symbol;
    
    // Add to list
    particle->next = engine->particles;
    engine->particles = particle;
}

void anim_particle_burst(AnimationEngine* engine, float x, float y, 
                        int count, float speed, uint32_t color) {
    for (int i = 0; i < count; i++) {
        float angle = (float)i / count * 2 * M_PI;
        float vx = cos(angle) * speed;
        float vy = sin(angle) * speed;
        char symbols[] = {'*', '.', 'o', '°'};
        anim_particle_spawn(engine, x, y, vx, vy, color, symbols[i % 4]);
    }
}

void anim_particle_update(AnimationEngine* engine) {
    if (!engine) return;
    
    Particle** current = &engine->particles;
    
    while (*current) {
        Particle* p = *current;
        
        // Update position
        p->x += p->vx * engine->time_scale;
        p->y += p->vy * engine->time_scale;
        
        // Apply gravity
        p->vy += 0.1f * engine->time_scale;
        
        // Update life
        p->life -= 0.02f * engine->time_scale;
        
        // Remove dead particles
        if (p->life <= 0) {
            *current = p->next;
            free(p);
        } else {
            current = &p->next;
        }
    }
}

// Screen effects
void anim_screen_shake(AnimationEngine* engine, float intensity, int duration) {
    if (!engine) return;
    
    engine->shake_intensity = intensity;
    engine->shake_duration = duration;
    engine->shake_counter = 0;
}

void anim_apply_shake(AnimationEngine* engine, int* x, int* y) {
    if (!engine || engine->shake_intensity <= 0) return;
    
    // Simple shake using sine waves
    float shake_x = sin(engine->shake_counter * 0.5f) * engine->shake_intensity;
    float shake_y = cos(engine->shake_counter * 0.7f) * engine->shake_intensity;
    
    *x += (int)shake_x;
    *y += (int)shake_y;
}

// Card animations
void anim_card_deal(CardAnimation* anim, int start_x, int start_y, 
                   int end_x, int end_y, float speed) {
    anim_transform_init(&anim->transform, start_x, start_y, end_x, end_y, speed);
    anim->rotation = 0;
    anim->scale = 1.0f;
    anim->flip_progress = 0;
    anim->face_up = false;
}

void anim_card_flip(CardAnimation* anim, bool to_face_up) {
    anim->face_up = to_face_up;
    anim->flip_progress = 0;
}

void anim_card_update(CardAnimation* anim) {
    // Update position
    anim_transform_update(&anim->transform, ease_out_cubic);
    
    // Update flip
    if (anim->flip_progress < 1.0f) {
        anim->flip_progress += 0.1f;
        if (anim->flip_progress > 1.0f) anim->flip_progress = 1.0f;
    }
    
    // Update rotation
    if (anim->rotation != 0) {
        anim->rotation *= 0.95f;  // Decay rotation
    }
}

// Chip animations
void anim_chip_bet(ChipAnimation* anim, int start_x, int start_y, 
                  int end_x, int end_y, int value) {
    anim_transform_init(&anim->transform, start_x, start_y, end_x, end_y, 0.05f);
    anim->value = value;
    anim->arc_height = 5.0f;
    
    // Set color based on value
    if (value >= 100) anim->color = 0x000000;      // Black
    else if (value >= 25) anim->color = 0x00FF00;  // Green
    else if (value >= 5) anim->color = 0xFF0000;   // Red
    else anim->color = 0xFFFFFF;                   // White
}

void anim_chip_update(ChipAnimation* anim) {
    bool complete = anim_transform_update(&anim->transform, ease_out_quad);
    
    // Add arc to trajectory
    if (!complete && anim->arc_height > 0) {
        float progress = anim->transform.progress;
        float arc = sin(progress * M_PI) * anim->arc_height;
        anim->transform.y -= arc;
    }
}

// Special poker effects
void anim_winner_celebration(AnimationEngine* engine, int x, int y) {
    // Burst of golden particles
    anim_particle_burst(engine, x, y, 20, 2.0f, 0xFFD700);
    
    // Screen flash effect would be implemented in the view
    
    // Add some sparkles around the winner
    for (int i = 0; i < 10; i++) {
        float offset_x = (rand() % 20) - 10;
        float offset_y = (rand() % 10) - 5;
        anim_particle_spawn(engine, x + offset_x, y + offset_y, 
                          0, -0.5f, 0xFFFFFF, '✨');
    }
}

void anim_fold_effect(AnimationEngine* engine, int x, int y) {
    // Cards fade and fall
    for (int i = 0; i < 5; i++) {
        float vx = (rand() % 10 - 5) * 0.1f;
        anim_particle_spawn(engine, x + i * 6, y, vx, 0.5f, 0x808080, '▒');
    }
}

// Timing utilities
float anim_get_frame_progress(int current_frame, int total_frames) {
    if (total_frames <= 0) return 1.0f;
    return (float)current_frame / total_frames;
}

int anim_frames_for_duration(int duration_ms, int fps) {
    return (duration_ms * fps) / 1000;
}

bool anim_is_complete(float progress) {
    return progress >= 1.0f;
}