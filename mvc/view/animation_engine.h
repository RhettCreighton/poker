#ifndef POKER_ANIMATION_ENGINE_H
#define POKER_ANIMATION_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

// Animation state for smooth movements
typedef struct {
    float x, y;           // Current position
    float start_x, start_y;
    float end_x, end_y;
    float progress;       // 0.0 to 1.0
    float speed;          // Units per frame
} AnimationTransform;

// Particle system for effects
typedef struct Particle {
    float x, y;
    float vx, vy;
    float life;
    uint32_t color;
    char symbol;
    struct Particle* next;
} Particle;

// Sprite for animated objects
typedef struct {
    char* frames[10];     // ASCII art frames
    int num_frames;
    int current_frame;
    int frame_delay;
    int frame_counter;
} Sprite;

// Core animation functions
typedef struct AnimationEngine {
    // Particle system
    Particle* particles;
    int max_particles;
    
    // Screen shake
    float shake_intensity;
    int shake_duration;
    int shake_counter;
    
    // Global time
    uint64_t frame_count;
    float time_scale;     // For slow-mo effects
    
    // Performance tracking
    float target_fps;
    int64_t last_frame_time;
    float current_fps;
    
    // Callbacks
    void (*on_animation_complete)(int anim_id, void* data);
    void* callback_data;
} AnimationEngine;

// Easing functions
float ease_linear(float t);
float ease_in_quad(float t);
float ease_out_quad(float t);
float ease_in_out_quad(float t);
float ease_in_cubic(float t);
float ease_out_cubic(float t);
float ease_in_out_cubic(float t);
float ease_in_elastic(float t);
float ease_out_elastic(float t);
float ease_out_bounce(float t);
float ease_out_back(float t);

// Engine management
AnimationEngine* anim_engine_create(void);
void anim_engine_destroy(AnimationEngine* engine);
void anim_engine_update(AnimationEngine* engine);
void anim_engine_set_target_fps(AnimationEngine* engine, float fps);
float anim_engine_get_current_fps(AnimationEngine* engine);

// Transform animations
void anim_transform_init(AnimationTransform* transform, float start_x, float start_y, 
                        float end_x, float end_y, float speed);
bool anim_transform_update(AnimationTransform* transform, float (*easing_func)(float));
void anim_transform_get_position(AnimationTransform* transform, int* x, int* y);

// Particle effects
void anim_particle_spawn(AnimationEngine* engine, float x, float y, 
                        float vx, float vy, uint32_t color, char symbol);
void anim_particle_burst(AnimationEngine* engine, float x, float y, 
                        int count, float speed, uint32_t color);
void anim_particle_fountain(AnimationEngine* engine, float x, float y, 
                           int rate, uint32_t color);
void anim_particle_trail(AnimationEngine* engine, float x1, float y1, 
                        float x2, float y2, uint32_t color);
void anim_particle_update(AnimationEngine* engine);
void anim_particle_render(AnimationEngine* engine, struct ncplane* plane);

// Screen effects
void anim_screen_shake(AnimationEngine* engine, float intensity, int duration);
void anim_screen_flash(struct ncplane* plane, uint32_t color, int duration);
void anim_screen_fade(struct ncplane* plane, bool fade_in, int duration);
void anim_apply_shake(AnimationEngine* engine, int* x, int* y);

// Card animations
typedef struct {
    AnimationTransform transform;
    float rotation;       // For spin effects
    float scale;          // For grow/shrink
    bool flip_progress;   // 0.0 to 1.0 for card flip
    bool face_up;
} CardAnimation;

void anim_card_deal(CardAnimation* anim, int start_x, int start_y, 
                   int end_x, int end_y, float speed);
void anim_card_flip(CardAnimation* anim, bool to_face_up);
void anim_card_discard(CardAnimation* anim, int target_x, int target_y);
void anim_card_spin(CardAnimation* anim, float spin_rate);
void anim_card_update(CardAnimation* anim);

// Chip animations
typedef struct {
    AnimationTransform transform;
    float arc_height;     // For arc trajectory
    int value;           // Chip denomination
    uint32_t color;
} ChipAnimation;

void anim_chip_bet(ChipAnimation* anim, int start_x, int start_y, 
                  int end_x, int end_y, int value);
void anim_chip_win(ChipAnimation* anim, int pot_x, int pot_y, 
                  int player_x, int player_y, float delay);
void anim_chip_stack(ChipAnimation* anims[], int count, int x, int y);
void anim_chip_update(ChipAnimation* anim);

// Text effects
void anim_text_typewriter(struct ncplane* plane, int x, int y, 
                         const char* text, int delay_ms);
void anim_text_fade_in(struct ncplane* plane, int x, int y, 
                      const char* text, int duration);
void anim_text_bounce(struct ncplane* plane, int x, int y, 
                     const char* text, float height);
void anim_text_wave(struct ncplane* plane, int x, int y, 
                   const char* text, float amplitude, float frequency);

// Sprite animations
Sprite* anim_sprite_create(const char* frames[], int num_frames, int frame_delay);
void anim_sprite_destroy(Sprite* sprite);
void anim_sprite_update(Sprite* sprite);
void anim_sprite_render(Sprite* sprite, struct ncplane* plane, int x, int y);

// Special effects for poker
void anim_deal_sequence(AnimationEngine* engine, int dealer_x, int dealer_y,
                       int player_positions[][2], int num_players, int cards_per_player);
void anim_pot_collection(AnimationEngine* engine, int pot_x, int pot_y,
                        int winner_x, int winner_y, int amount);
void anim_showdown_reveal(AnimationEngine* engine, int positions[][2], 
                         int num_players, float delay_between);
void anim_winner_celebration(AnimationEngine* engine, int x, int y);
void anim_fold_effect(AnimationEngine* engine, int x, int y);
void anim_all_in_push(AnimationEngine* engine, int player_x, int player_y,
                     int pot_x, int pot_y);

// Timing utilities
float anim_get_frame_progress(int current_frame, int total_frames);
int anim_frames_for_duration(int duration_ms, int fps);
bool anim_is_complete(float progress);

// Debug visualization
void anim_debug_draw_path(struct ncplane* plane, AnimationTransform* transform);
void anim_debug_show_particles(AnimationEngine* engine, struct ncplane* plane);

#endif // POKER_ANIMATION_ENGINE_H