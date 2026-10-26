#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "memory.hpp"
namespace point_zero::native {
namespace build_bindings {
struct AssetContract {
    std::uint32_t load_rva = 0x33BF60;
    std::uint32_t release_rva = 0x33B010;
    std::uint32_t manager_rva = 0xC2EAED0;
};
struct BootText {
    std::uint32_t renderer_rva = 0x5DD170;
    std::uint32_t capture_rva = 0x926979;
    std::array<std::byte, 19> capture{
        std::byte{0x48}, std::byte{0xC7}, std::byte{0x44}, std::byte{0x24}, std::byte{0x20},
        std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{0xE8},
        std::byte{0x89}, std::byte{0x68}, std::byte{0xCB}, std::byte{0xFF}, std::byte{0x4C},
        std::byte{0x8D}, std::byte{0x5C}, std::byte{0x24}, std::byte{0x50}};
};
struct Browsing {
    std::uint32_t lookup_rva = 0x85B8D0;
    std::uint32_t bank_sort_rva = 0x85D650;
    std::uint32_t chart_difficulty_rva = 0x5EB7E0;
    std::uint32_t style_rva = 0x930180;
    std::uint32_t options_rva = 0x871AA0;
    std::uint32_t battle_rva = 0x884B70;
    std::uint32_t keypad_rva = 0x7EA790;
    std::uint32_t modal_rva = 0x7F0880;
    std::uint32_t list_state_rva = 0x7E49E0;
    std::uint32_t reset_list_rva = 0x7E4990;
};
struct CatalogContract {
    std::uint32_t catalog_getters_rva = 0x939430;
    std::uint32_t catalog_load_rva = 0x93A080;
};
struct CategoryAccessorsContract {
struct Pin { std::uint32_t begin, end; };
    std::array<Pin, 8> pins{{
    {0x85b8d0,0x85b945},
    {0x85b950,0x85b995},
    {0x85c010,0x85c076},
    {0x85c080,0x85c0ea},
    {0x85c1e0,0x85c21c},
    {0x85c330,0x85c36c},
    {0x85c840,0x85c87c},
    {0x85c990,0x85c9cc},
}};
    std::uint32_t category_style_rva = 0x85BCD0;
};
struct CategoryBadgesContract {
    std::uint32_t music_owner_rva = 0x871A30;
    std::uint32_t event_owner_rva = 0x871A10;
    std::uint32_t chart_at_rva = 0x5EE6F0;
    std::uint32_t music_of_rva = 0x5EB580;
    std::uint32_t weekly_rva = 0x8815E0;
    std::uint32_t recommend_rva = 0x8811D0;
    std::uint32_t shop_rva = 0x85C880;
    std::uint32_t kac_rva = 0x8750A0;
};
struct CategoryGenerations {
    std::uint32_t category_manager_rva = 0xB3E91D8;
    std::uint32_t scene_vtable_rva = 0xD517B8;
    std::uint32_t dp_flag_rva = 0xB60A5D4;
    std::uint32_t active_flags_rva = 0xB60A5E0;
    std::uint32_t idle_callback_rva = 0x8DA460;
    std::uint32_t filter_restore_caller_rva = 0x5EFAD0;
};
struct CategoryLifecycleContract {
struct Pin { std::uint32_t begin, end; };
std::array<Pin, 13> pins{{
    {0x8DA840, 0x8DA892},
    {0x628880, 0x62897D},
    {0x629810, 0x62989F},
    {0x628370, 0x6283FB},
    {0x5F6A50, 0x5F6DE9},
    {0x671300, 0x671433},
    {0x8D6B10, 0x8D6DE3},
    {0x8D7F70, 0x8D8358},
    {0x8D8650, 0x8D887A},
    {0x8D9020, 0x8D90D4},
    {0x7EA590, 0x7EA5DF},
    {0x5FACF0, 0x5FADB7},
    {0x5FAB40, 0x5FAB4E},
}};
Pin queued_restore{0x8D7120, 0x8D7652};
std::uint32_t queue_caller_rva = 0xA6E536;
std::uint32_t initialize_vtable_rva = 0xD51998;
std::uint32_t restore_vtable_rva = 0xD519D0;
std::uint32_t restore_invoke_rva = 0x8DAA10;
std::uint32_t restore_caller_rva = 0x8D762F;
std::uint32_t construction_caller_rva = 0x62BD68;
std::uint32_t sites_initialize_caller_rva = 0x5FA126;
std::uint32_t sites_scene_entry_caller_rva = 0x95815C;
std::uint32_t sites_scene_tick_caller_rva = 0x9586CE;
std::uint32_t sites_scene_refresh_caller_rva = 0x8D8800;
std::uint32_t sites_preview_caller_rva = 0x5FB7B1;
std::uint32_t sites_style_apply_caller_rva = 0x8D9091;
std::uint32_t sites_style_restore_caller_rva = 0x8D90BF;
std::uint32_t scene_vtable_rva = 0xD517B8;
std::uint32_t idle_callback_rva = 0x8DA460;
};
struct CategoryMetadataContract {
    std::uint32_t game_rva = 0x85B9A0;
};
struct CategoryNavigation {
    std::uint32_t category_manager_rva = 0xB3E91D8;
};
struct CategoryNavigationContract {
    std::uint32_t open_rva = 0x5F6470;
    std::uint32_t close_rva = 0x5F6380;
    std::uint32_t sound_rva = 0x5B7B60;
    std::uint32_t exact_rva = 0x5EF4C0;
    std::uint32_t restore_chart_rva = 0x5E9640;
    std::uint32_t set_difficulty_rva = 0x5EFB70;
    std::uint32_t cycle_slot = 0xC831C8;
    std::uint32_t sites_cycle_caller_rva = 0x5F6F9D;
    std::uint32_t sites_back_rva = 0x5F71DF;
    std::uint32_t sites_back_continuation_rva = 0x5F7842;
};
struct CategoryRequestContract {
    std::uint32_t settings_rva = 0x871A40;
    std::uint32_t persistent_rva = 0x85C420;
    std::uint32_t store_rva = 0x86FA10;
    std::uint32_t save_rva = 0x6CEF80;
    std::uint32_t clear_rva = 0x6D0870;
    std::uint32_t group_rva = 0x62A820;
    std::uint32_t sound_rva = 0x5B7B60;
};
struct CategoryRequestSitesContract {
struct Pin { std::uint32_t begin, end;  };
    std::array<Pin, 4> pins{{
    {0x5fc0e0,0x5fc2f8},
    {0x5fc300,0x5fc71d},
    {0x5fc720,0x5fc9db},
    {0x86fa00,0x86fa08},
}};
    std::array<std::uint32_t, 3> captures{0x5FC138,0x5FC4B2,0x5FC82A};
    std::array<std::uint32_t, 3> cancels{0x5FC2AE,0x5FC5CF,0x5FC991};
    std::array<std::uint32_t, 3> callers{0x5FA6BA,0x5FA6A2,0x5FA6AE};
};
struct CategoryRestoreContract {
    std::uint32_t current_rva = 0x5F6760;
    std::uint32_t restore_rva = 0x5FACF0;
};
struct CategoryRoots {
    std::uint32_t category_manager_rva = 0xB3E91D8;
    std::uint32_t category_storage_rva = 0xB3E91E0;
};
struct CategoryStorageContract {
    std::uint32_t construct_rva = 0x5EDC80;
    std::uint32_t initialize_rva = 0x5EEB30;
    std::uint32_t assign_string_rva = 0x33E620;
    std::uint32_t insert_rva = 0x85ACA0;
    std::uint32_t destroy_rva = 0x86F350;
    std::uint32_t auxiliary_vtable_rva = 0xC80B38;
};
struct CategorySummaryContract {
    std::uint32_t category_summary_rva = 0x85CE90;
    std::uint32_t summary_capture_rva = 0x85CEF7;
    std::uint32_t summary_continuation_rva = 0x85D0A7;
};
struct CategoryVectorContract {
    std::uint32_t grow_rva = 0x6285C0;
    std::uint32_t destroy_rva = 0x628300;
};
struct DifficultySort {
    std::uint32_t store_rva = 0xB3E91D8;
    std::uint32_t style_flag_rva = 0xB3E8D80;
    std::uint32_t visible_bars_rva = 0x1038480;
    std::uint32_t active_flags_rva = 0xB60A5E0;
    std::uint32_t dp_flag_rva = 0xB60A5D4;
    std::uint32_t sort_return_rva = 0x85D878;
};
struct FilterContract {
struct Pin { std::uint32_t rva, size; };
    std::array<Pin, 3> code_pins{{
    {0x6D0A20,131},
    {0x6D0850,32},
    {0x880D30,506},
}};
    std::uint32_t filter_apply_rva = 0x6D0A20;
    std::uint32_t filter_level_rva = 0x6D0850;
    std::uint32_t filter_candidate_rva = 0x880D30;
    std::uint32_t filter_apply_caller_rva = 0x880E0A;
    std::uint32_t sites_level_caller_rva = 0x6D0A73;
    std::uint32_t mode_rva = 0x92FFD0;
    std::uint32_t rebuild_rva = 0x5EF6D0;
};
struct GameDriver {
    std::uint32_t graphics_started_rva = 0xBCDC000;
    std::uint32_t catalog_mutex_rva = 0xC38E6D0;
};
struct Guide {
struct Pin { std::uint32_t rva, size; const char* sha; };
    Pin builder{0x66C810, 1680, nullptr};
    Pin capture{0x66D5A0, 6,
    "2c96a8932afbbb93e85faec5ddbd8df4259e98c6ef97e35f91a890c503c26b48"};
    Pin copy_string{0x357E20, 205, nullptr};
    Pin grow_vector{0x400AE0, 549, nullptr};
    std::uint32_t guide_build_caller_rva = 0x66D304;
    std::uint32_t guide_create_call_rva = 0x66D5A6;
};
struct LevelPopulation {
    std::uint32_t category_manager_rva = 0xB3E91D8;
};
struct List {
    std::uint32_t pool_rva = 0xBE12430;
    std::uint32_t background_return_rva = 0x5E98C8;
    std::uint32_t p1_return_rva = 0x5EA48C;
    std::uint32_t p2_return_rva = 0x5EA4DB;
    std::uint32_t solo_return_rva = 0x5EA59D;
};
struct ListChart {
    std::uint32_t p1_site = 0x5EA48C;
    std::uint32_t p2_site = 0x5EA4DB;
    std::uint32_t solo_site = 0x5EA59D;
};
struct ListContract {
    std::uint32_t chart_player_count_rva = 0x930090;
    std::uint32_t chart_style_rva = 0x930140;
    std::uint32_t chart_active_player_rva = 0x92FE40;
    std::uint32_t emit_rva = 0x3350F0;
};
struct ListHookContract {
    std::uint32_t background_return_rva = 0x5E98C8;
    std::uint32_t p1_return_rva = 0x5EA48C;
    std::uint32_t p2_return_rva = 0x5EA4DB;
    std::uint32_t solo_return_rva = 0x5EA59D;
};
struct ListTitleContract {
struct Pin { std::uint32_t rva, size; };
    Pin title{0x5EA770, 0x309};
    std::uint32_t title_background_return_rva = 0x5E98E1;
    std::uint32_t title_width_end_rva = 0x5EAA07;
    std::uint32_t title_entry_rva = 0x5EA770;
};
struct OriginalFilter {
    std::uint32_t filter_candidate_state_rva = 0xB4D4388;
    std::uint32_t filter_apply_caller_rva = 0x880E0A;
    std::uint32_t category_manager_rva = 0xB3E91D8;
    std::uint32_t filter_state_end_rva = 0xB3E9368;
    std::uint32_t list_vtable_rva = 0xC831C0;
    std::uint32_t active_filter_rva = 0xB3E9358;
    std::uint32_t saved_filter_rva = 0xB3E9360;
    std::uint32_t filter_restore_caller_rva = 0x5EFAD0;
};
struct OriginalFilterState {
    std::uint32_t saved_filter_rva = 0xB3E9360;
    std::uint32_t category_manager_rva = 0xB3E91D8;
    std::uint32_t list_vtable_rva = 0xC831C0;
    std::uint32_t active_filter_rva = 0xB3E9358;
    std::uint32_t filter_vtable_rva = 0xC94D00;
};
struct PlayedCallsContract {
    std::uint32_t relative_import_rva = 0xC65CB0;
    std::uint32_t property_import_rva = 0xC65C10;
    std::uint32_t chart_style_rva = 0x930140;
};
struct PlayedChart {
    std::uint32_t active_flags = 0xB60A5E0;
    std::uint32_t dp_flag = 0xB60A5D4;
    std::uint32_t published_music = 0xB60A600;
    std::uint32_t published_difficulties = 0xB60A5D8;
    std::uint32_t hud_site = 0x8EF69D;
    std::uint32_t result_site = 0x9075ED;
    std::uint32_t stepup_site = 0x5F5710;
    std::uint32_t resource_slots = 0xC2EAED0;
};
struct PlayedContract {
struct Pin { std::uint32_t rva, size; };
    std::array<Pin, 5> pins{{
    {0x3298B0,171},
    {0x8EF510,478},
    {0x9071A0,1269},
    {0x820BB0,50},
    {0x90E5C0,139},
}};
    std::uint32_t hud_capture_rva = 0x8EF69D;
    std::uint32_t result_capture_rva = 0x9075ED;
    std::uint32_t hud_return_rva = 0x8EF6B0;
    std::uint32_t named_return_rva = 0x820BDC;
    std::uint32_t notes_capture_rva = 0x90E5DA;
    std::uint32_t result_return_first_rva = 0x907625;
    std::uint32_t result_return_second_rva = 0x90763A;
};
struct SortContract {
    std::uint32_t mode_rva = 0x92FFD0;
    std::uint32_t level_rva = 0x85DC70;
    std::uint32_t sort_return_rva = 0x85D878;
};
struct StepupContract {
struct Pin { std::uint32_t rva, size; };
    Pin builder{0x5F5650, 227};
    std::uint32_t capture_rva = 0x5F5710;
    std::uint32_t named_return_rva = 0x5F571D;
};
struct DanContract {
    PlayedContract::Pin builder{0x8B9190, 0x664};
    std::uint32_t capture_rva = 0x8B93B4;
    std::uint32_t named_return_rva = 0x8B93C3;
    std::uint32_t scene_stack_offset = 0x50;
    std::uint32_t scene_layer_offset = 0x6E8;
};
struct SurfaceContract {
    std::uint32_t cup_rva = 0x5F2080;
    std::uint32_t bitmap_import = 0xC65C00;
    std::uint32_t cup_setter_rva = 0x5F27A0;
    std::uint32_t confirmation_setter_rva = 0x5F3940;
    std::uint32_t confirmation_rva = 0x5F3210;
};
struct PrimaryContract {
    std::uint32_t style_rva = 0x930140;
    std::uint32_t clock_rva = 0x922B40;
    std::uint32_t rate_rva = 0xA3CB40;
};
struct ConfirmationContract {
    std::uint32_t style_rva = 0x930140;
    std::uint32_t logical_count_rva = 0x930090;
    std::uint32_t sole_player_rva = 0x92FE40;
    std::uint32_t raw_mode_rva = 0x92FFB0;
    std::uint32_t published_music_rva = 0xB60A600;
    std::uint32_t published_difficulties_rva = 0xB60A5D8;
    std::uint32_t name_table_rva = 0x101EFF0;
    std::array<std::uint32_t, 2> setter_callers{0x8D97D1, 0x8D97F1};
    std::array<std::uint32_t, 2> named_callers{0x5F3325, 0x5F3359};
    std::uint32_t builder_caller_rva = 0x8D880C;
};
struct AssistantHeaderContract {
    std::uint32_t update_rva = 0x5FA902;
    std::uint32_t header_rva = 0x600DC3;
    std::uint32_t style_rva = 0x930140;
    std::uint32_t player_rva = 0x92FE40;
};
}
struct CaptureBindings {
    std::array<std::byte, 7> dan{std::byte{0x48},std::byte{0x8d},std::byte{0x95},
        std::byte{0x70},std::byte{0x01},std::byte{},std::byte{}};
    std::array<std::byte, 8> stepup{std::byte{0x4e},std::byte{0x8b},std::byte{0x84},std::byte{0xc3},
        std::byte{0xf0},std::byte{0xf2},std::byte{0x01},std::byte{0x01}};
};
struct GameBindings {
    CaptureBindings captures;
    build_bindings::AssetContract asset_contract;
    build_bindings::BootText boot_text;
    build_bindings::Browsing browsing;
    build_bindings::CatalogContract catalog_contract;
    build_bindings::CategoryAccessorsContract category_accessors_contract;
    build_bindings::CategoryBadgesContract category_badges_contract;
    build_bindings::CategoryGenerations category_generations;
    build_bindings::CategoryLifecycleContract category_lifecycle_contract;
    build_bindings::CategoryMetadataContract category_metadata_contract;
    build_bindings::CategoryNavigation category_navigation;
    build_bindings::CategoryNavigationContract category_navigation_contract;
    build_bindings::CategoryRequestContract category_request_contract;
    build_bindings::CategoryRequestSitesContract category_request_sites_contract;
    build_bindings::CategoryRestoreContract category_restore_contract;
    build_bindings::CategoryRoots category_roots;
    build_bindings::CategoryStorageContract category_storage_contract;
    build_bindings::CategorySummaryContract category_summary_contract;
    build_bindings::CategoryVectorContract category_vector_contract;
    build_bindings::DifficultySort difficulty_sort;
    build_bindings::FilterContract filter_contract;
    build_bindings::GameDriver game_driver;
    build_bindings::Guide guide;
    build_bindings::LevelPopulation level_population;
    build_bindings::List list;
    build_bindings::ListChart list_chart;
    build_bindings::ListContract list_contract;
    build_bindings::ListHookContract list_hook_contract;
    build_bindings::ListTitleContract list_title_contract;
    build_bindings::OriginalFilter original_filter;
    build_bindings::OriginalFilterState original_filter_state;
    build_bindings::PlayedCallsContract played_calls_contract;
    build_bindings::PlayedChart played_chart;
    build_bindings::PlayedContract played_contract;
    build_bindings::SortContract sort_contract;
    build_bindings::StepupContract stepup_contract;
    build_bindings::DanContract dan_contract;
    build_bindings::SurfaceContract surface_contract;
    build_bindings::PrimaryContract primary_contract;
    build_bindings::ConfirmationContract confirmation_contract;
    build_bindings::AssistantHeaderContract assistant_header_contract;
};
extern const GameBindings october_bindings;
extern const GameBindings september16_bindings;
extern const GameBindings september16_012_bindings;
extern const GameBindings september30_bindings;
inline const GameBindings& game_bindings(ImageRange image) noexcept { return *image.bindings; }
}
