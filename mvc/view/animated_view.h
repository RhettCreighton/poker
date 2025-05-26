#ifndef ANIMATED_VIEW_H
#define ANIMATED_VIEW_H

#include "beautiful_view.h"

// Animation state tracking
typedef struct {
    bool is_animating;
    int animation_type;
    int animation_frame;
    int total_frames;
    
    // Card animation data
    struct {
        int player;
        int cards_to_replace[5];
        ViewCard old_cards[5];
        bool has_old_cards;
    } card_anim;
    
    // Chip animation data
    struct {
        int player;
        int amount;
        int num_chips;
        int current_chip;
    } chip_anim;
    
    // Action flash data
    struct {
        int player;
        char action_type[32];
        int flash_count;
    } action_flash;
} AnimationState;

// Enhanced view with animations
typedef struct AnimatedView {
    BeautifulView* base_view;
    AnimationState anim_state;
    
    // Animation settings
    bool animations_enabled;
    float animation_speed;  // 1.0 = normal, 2.0 = double speed
} AnimatedView;

// Animation types
#define ANIM_NONE 0
#define ANIM_CARD_REPLACEMENT 1
#define ANIM_CHIP_TO_POT 2
#define ANIM_ACTION_FLASH 3
#define ANIM_DEAL_CARDS 4

// Create/destroy animated view
AnimatedView* animated_view_create(struct notcurses* nc);
void animated_view_destroy(AnimatedView* view);

// Animation control
void animated_view_enable_animations(AnimatedView* view, bool enable);
void animated_view_set_speed(AnimatedView* view, float speed);
bool animated_view_is_animating(AnimatedView* view);
void animated_view_update(AnimatedView* view);

// Rendering with animations
void animated_view_render_scene(AnimatedView* view, ViewGameState* game);
void animated_view_render_scene_with_hidden_cards(AnimatedView* view, ViewGameState* game,
                                                  int hide_player, int* hide_cards);

// Start animations (from demo code)
void animated_view_start_card_replacement(AnimatedView* view, ViewGameState* game,
                                         int player_idx, int* cards_to_replace, 
                                         ViewCard* old_cards);
void animated_view_start_chip_animation(AnimatedView* view, ViewGameState* game,
                                      int player_idx, int amount);
void animated_view_start_action_flash(AnimatedView* view, int player_idx, 
                                    const char* action_type);
void animated_view_start_deal_animation(AnimatedView* view, ViewGameState* game);

// Animation rendering functions (extracted from demos)
void animated_view_render_card_replacement(AnimatedView* view, ViewGameState* game);
void animated_view_render_chip_animation(AnimatedView* view, ViewGameState* game);
void animated_view_render_action_flash(AnimatedView* view, ViewGameState* game);
void animated_view_render_deal_cards(AnimatedView* view, ViewGameState* game);

// Easing functions (from demos)
float animated_view_ease_in_out(float t);
float animated_view_ease_out_bounce(float t);

// Background preservation utility (from chip animation)
void animated_view_draw_preserving_background(AnimatedView* view, int y, int x, 
                                            const char* symbol, uint32_t fg_color);

// Player positioning for 9-player layout
void animated_view_position_9_players(AnimatedView* view, ViewGameState* game);

#endif // ANIMATED_VIEW_H