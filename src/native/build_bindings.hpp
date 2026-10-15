#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "memory.hpp"
namespace point_zero::native {
namespace build_bindings {
struct AssetContract {
    std::uint32_t load_rva = 0x3519A0;
    std::uint32_t release_rva = 0x3507E0;
    std::uint32_t manager_rva = 0xB8B7E90;
};
struct BootText {
    std::uint32_t renderer_rva = 0x5EB470;
    std::uint32_t capture_rva = 0x93F629;
};
struct Browsing {
    std::uint32_t lookup_rva = 0x86EE30;
    std::uint32_t bank_sort_rva = 0x870D60;
    std::uint32_t chart_difficulty_rva = 0x5F9B00;
    std::uint32_t style_rva = 0x949570;
    std::uint32_t options_rva = 0x8854A0;
    std::uint32_t battle_rva = 0x897BB0;
    std::uint32_t keypad_rva = 0x800E70;
    std::uint32_t modal_rva = 0x806F60;
    std::uint32_t list_state_rva = 0x7FB4B0;
    std::uint32_t reset_list_rva = 0x7FB460;
};
struct CatalogContract {
    std::uint32_t catalog_getters_rva = 0x951fd0;
    std::uint32_t catalog_load_rva = 0x952c20;
};
struct CategoryAccessorsContract {
struct Pin { std::uint32_t begin, end; };
    std::array<Pin, 8> pins{{
    {0x86EE30,0x86EEA5},
    {0x86EEB0,0x86EEF5},
    {0x86F570,0x86F5D6},
    {0x86F5E0,0x86F64D},
    {0x86F750,0x86F78C},
    {0x86F8A0,0x86F8DC},
    {0x86FDB0,0x86FDEC},
    {0x86FF00,0x86FF3C},
}};
    std::uint32_t category_style_rva = 0x86F230;
};
struct CategoryBadgesContract {
    std::uint32_t music_owner_rva = 0x885430;
    std::uint32_t event_owner_rva = 0x885400;
    std::uint32_t chart_at_rva = 0x5FC690;
    std::uint32_t music_of_rva = 0x5F97A0;
    std::uint32_t weekly_rva = 0x894810;
    std::uint32_t recommend_rva = 0x894450;
    std::uint32_t shop_rva = 0x86FDF0;
    std::uint32_t kac_rva = 0x888C30;
};
struct CategoryGenerations {
    std::uint32_t category_manager_rva = 0xAAB3B08;
    std::uint32_t scene_vtable_rva = 0xD84788;
    std::uint32_t dp_flag_rva = 0xACD79A4;
    std::uint32_t active_flags_rva = 0xACD79B0;
    std::uint32_t idle_callback_rva = 0x8EE190;
    std::uint32_t filter_restore_caller_rva = 0x5FDB25;
};
struct CategoryLifecycleContract {
struct Pin { std::uint32_t begin, end; };
    std::array<Pin, 13> pins{{
    {0x86FFF0, 0x87009F},
    {0x638A70, 0x638B7D},
    {0x639A30, 0x639ABF},
    {0x611B80, 0x611C0B},
    {0x603EE0, 0x604109},
    {0x687E20, 0x687F53},
    {0x8EA7E0, 0x8EA9E2},
    {0x8EB050, 0x8EB81B},
    {0x8EC1F0, 0x8EC44A},
    {0x8EC750, 0x8EC804},
    {0x800C70, 0x800CBF},
    {0x607F30, 0x607FF7},
    {0x607D80, 0x607D8E},
}};
    std::uint32_t sites_initialize_caller_rva = 0x60717B;
    std::uint32_t sites_scene_entry_caller_rva = 0x8EB930;
    std::uint32_t sites_scene_tick_caller_rva = 0x97120B;
    std::uint32_t sites_scene_refresh_caller_rva = 0x8EC3D1;
    std::uint32_t sites_preview_caller_rva = 0x608A7A;
    std::uint32_t sites_restore_caller_rva = 0x8EB6F0;
    std::uint32_t sites_style_apply_caller_rva = 0x8EC7C1;
    std::uint32_t sites_style_restore_caller_rva = 0x8EC7EF;
    std::uint32_t scene_vtable_rva = 0xD84788;
    std::uint32_t idle_callback_rva = 0x8EE190;
};
struct CategoryMetadataContract {
    std::uint32_t game_rva = 0x86EF00;
};
struct CategoryNavigation {
    std::uint32_t category_manager_rva = 0xAAB3B08;
};
struct CategoryNavigationContract {
    std::uint32_t open_rva = 0x603900;
    std::uint32_t close_rva = 0x603810;
    std::uint32_t sound_rva = 0x5C5C80;
    std::uint32_t exact_rva = 0x5FD520;
    std::uint32_t restore_chart_rva = 0x5F7BE0;
    std::uint32_t set_difficulty_rva = 0x5FDBD0;
    std::uint32_t cycle_slot = 0xD53280;
    std::uint32_t sites_cycle_caller_rva = 0x6042BD;
    std::uint32_t sites_back_rva = 0x60451E;
    std::uint32_t sites_back_continuation_rva = 0x604B9F;
};
struct CategoryRequestContract {
    std::uint32_t settings_rva = 0x885440;
    std::uint32_t persistent_rva = 0x86F990;
    std::uint32_t store_rva = 0x883290;
    std::uint32_t save_rva = 0x6E7570;
    std::uint32_t clear_rva = 0x6E8E60;
    std::uint32_t group_rva = 0x63AAF0;
    std::uint32_t sound_rva = 0x5C5C80;
};
struct CategoryRequestSitesContract {
struct Pin { std::uint32_t begin, end;  };
    std::array<Pin, 6> pins{{
    {0x609280, 0x609498},
    {0x6094A0, 0x6098BD},
    {0x6098C0, 0x609B7B},
    {0x6077D0, 0x607CC0},
    {0x883280, 0x883288},
    {0x8EC450, 0x8EC742},
}};
    std::array<std::uint32_t, 3> captures{0x6092D8, 0x609652, 0x6099CA};
    std::array<std::uint32_t, 3> cancels{0x60944E, 0x60976F, 0x609B31};
    std::array<std::uint32_t, 3> callers{0x6078AC, 0x60788C, 0x60789C};
    std::uint32_t category_manager_rva = 0xAAB3B08;
};
struct CategoryRestoreContract {
    std::uint32_t current_rva = 0x603BF0;
    std::uint32_t restore_rva = 0x607F30;
};
struct CategoryRoots {
    std::uint32_t category_manager_rva = 0xAAB3B08;
    std::uint32_t category_storage_rva = 0xAAB3B10;
};
struct CategoryStorageContract {
    std::uint32_t construct_rva = 0x5FBC10;
    std::uint32_t initialize_rva = 0x5FCAD0;
    std::uint32_t assign_string_rva = 0x366B00;
    std::uint32_t insert_rva = 0x86E170;
    std::uint32_t destroy_rva = 0x882BD0;
    std::uint32_t auxiliary_vtable_rva = 0xCACDA0;
};
struct CategorySummaryContract {
    std::uint32_t category_summary_rva = 0x8704B0;
    std::uint32_t summary_capture_rva = 0x870517;
    std::uint32_t summary_continuation_rva = 0x8706C7;
};
struct CategoryVectorContract {
    std::uint32_t grow_rva = 0x6387B0;
    std::uint32_t destroy_rva = 0x611B20;
};
struct Cup {
    std::uint32_t layer_vtable = 0xDC86E0;
    std::uint32_t animation_clock_rva = 0xABAF550;
};
struct DifficultySort {
    std::uint32_t store_rva = 0xAAB3B08;
    std::uint32_t style_flag_rva = 0xAAB36B0;
    std::uint32_t visible_bars_rva = 0x1077540;
    std::uint32_t active_flags_rva = 0xACD79B0;
    std::uint32_t dp_flag_rva = 0xACD79A4;
    std::uint32_t sort_return_rva = 0x870F88;
};
struct FilterContract {
struct Pin { std::uint32_t rva, size; };
    std::array<Pin, 3> code_pins{{
    {0x6E9010,131},
    {0x6E8E40,32},
    {0x893FA0,506},
}};
    std::uint32_t filter_apply_rva = 0x6E9010;
    std::uint32_t filter_level_rva = 0x6E8E40;
    std::uint32_t filter_candidate_rva = 0x893FA0;
    std::uint32_t filter_apply_caller_rva = 0x89407A;
    std::uint32_t sites_level_caller_rva = 0x6E9063;
    std::uint32_t mode_rva = 0x9493C0;
    std::uint32_t rebuild_rva = 0x5FD730;
};
struct GameDriver {
    std::uint32_t graphics_started_rva = 0xB2A9000;
    std::uint32_t catalog_mutex_rva = 0xB95B690;
};
struct Guide {
struct Pin { std::uint32_t rva, size; const char* sha; };
    Pin builder{0x6833A0, 1632, nullptr};
    Pin capture{0x6840E0, 6,
    "2c96a8932afbbb93e85faec5ddbd8df4259e98c6ef97e35f91a890c503c26b48"};
    Pin copy_string{0x369660, 205, nullptr};
    Pin grow_vector{0x4137D0, 549, nullptr};
    std::uint32_t guide_build_caller_rva = 0x683E44;
    std::uint32_t guide_create_call_rva = 0x6840E6;
};
struct LevelPopulation {
    std::uint32_t category_manager_rva = 0xAAB3B08;
};
struct List {
    std::uint32_t pool_rva = 0xB3DF3E0;
    std::uint32_t background_return_rva = 0x5F7E62;
    std::uint32_t p1_return_rva = 0x5F865C;
    std::uint32_t p2_return_rva = 0x5F86AB;
    std::uint32_t solo_return_rva = 0x5F876D;
};
struct ListChart {
    std::uint32_t p1_site = 0x5F865C;
    std::uint32_t p2_site = 0x5F86AB;
    std::uint32_t solo_site = 0x5F876D;
};
struct ListContract {
    std::uint32_t chart_player_count_rva = 0x949480;
    std::uint32_t chart_style_rva = 0x949530;
    std::uint32_t chart_active_player_rva = 0x949230;
    std::uint32_t emit_rva = 0x34B360;
};
struct ListHookContract {
    std::uint32_t background_return_rva = 0x5F7E62;
    std::uint32_t p1_return_rva = 0x5F865C;
    std::uint32_t p2_return_rva = 0x5F86AB;
    std::uint32_t solo_return_rva = 0x5F876D;
};
struct ListTitleContract {
struct Pin { std::uint32_t rva, size; };
    Pin title{0x5F8AC0, 874};
    std::uint32_t title_background_return_rva = 0x5F7E7A;
    std::uint32_t title_width_end_rva = 0x5F8DB8;
    std::uint32_t title_entry_rva = 0x5F8AC0;
};
struct OriginalFilter {
    std::uint32_t filter_candidate_state_rva = 0xAB9EE78;
    std::uint32_t filter_apply_caller_rva = 0x89407A;
    std::uint32_t category_manager_rva = 0xAAB3B08;
    std::uint32_t filter_state_end_rva = 0xAAB3CA0;
    std::uint32_t list_vtable_rva = 0xD53278;
    std::uint32_t active_filter_rva = 0xAAB3C90;
    std::uint32_t saved_filter_rva = 0xAAB3C98;
    std::uint32_t filter_restore_caller_rva = 0x5FDB25;
};
struct OriginalFilterState {
    std::uint32_t saved_filter_rva = 0xAAB3C98;
    std::uint32_t category_manager_rva = 0xAAB3B08;
    std::uint32_t list_vtable_rva = 0xD53278;
    std::uint32_t active_filter_rva = 0xAAB3C90;
    std::uint32_t filter_vtable_rva = 0xCC1490;
};
struct PlayedCallsContract {
    std::uint32_t relative_import_rva = 0xC91B88;
    std::uint32_t property_import_rva = 0xC91BC8;
    std::uint32_t chart_style_rva = 0x949530;
};
struct PlayedChart {
    std::uint32_t active_flags = 0xACD79B0;
    std::uint32_t dp_flag = 0xACD79A4;
    std::uint32_t published_music = 0xACD79D0;
    std::uint32_t published_difficulties = 0xACD79A8;
    std::uint32_t decide_first = 0x601715;
    std::uint32_t decide_second = 0x601749;
    std::uint32_t hud_site = 0x9045FD;
    std::uint32_t result_site = 0x91C56D;
    std::uint32_t stepup_site = 0x601F4E;
    std::uint32_t resource_slots = 0xB8B7E90;
};
struct PlayedContract {
struct Pin { std::uint32_t rva, size; };
    std::array<Pin, 6> pins{{
    {0x340390, 171},
    {0x601610, 1931},
    {0x904470, 478},
    {0x91C120, 1269},
    {0x834510, 50},
    {0x925980, 139},
}};
    std::uint32_t confirmation_first_rva = 0x601715;
    std::uint32_t confirmation_second_rva = 0x601749;
    std::uint32_t hud_capture_rva = 0x9045FD;
    std::uint32_t result_capture_rva = 0x91C56D;
    std::uint32_t confirmation_first_return_rva = 0x601725;
    std::uint32_t confirmation_second_return_rva = 0x601759;
    std::uint32_t hud_return_rva = 0x904610;
    std::uint32_t named_return_rva = 0x83453C;
    std::uint32_t notes_capture_rva = 0x92599A;
    std::uint32_t result_return_first_rva = 0x91C5A5;
    std::uint32_t result_return_second_rva = 0x91C5BA;
};
struct SongSelect {
    std::uint32_t category_data = 0xAAB3B08;
    std::uint32_t category_list = 0xD53278;
    std::array<std::uintptr_t, 12> groups{
    0xCB6D88, 0xCB6B10, 0xCB70B0, 0xCB6BC0, 0xCB6C70, 0xCB6478,
    0xCB63D0, 0, 0xCB69D0, 0xCB6E60, 0xCB6A90, 0xCB0700};
};
struct SortContract {
    std::uint32_t mode_rva = 0x9493C0;
    std::uint32_t level_rva = 0x871250;
    std::uint32_t sort_return_rva = 0x870F88;
};
struct StepupContract {
struct Pin { std::uint32_t rva, size; };
    Pin builder{0x601E80, 522};
    std::uint32_t capture_rva = 0x601F4E;
    std::uint32_t named_return_rva = 0x601F5B;
};
struct SurfaceContract {
    std::uint32_t cup_rva = 0x6001D0;
    std::uint32_t bitmap_import = 0xC91BD8;
};
}
struct CaptureBindings {
    std::array<std::byte, 8> confirmation{std::byte{0x48},std::byte{0x8b},std::byte{0x94},std::byte{0xd3},
        std::byte{0xf0},std::byte{0xdf},std::byte{0x05},std::byte{0x01}};
    std::array<std::byte, 8> stepup{std::byte{0x4f},std::byte{0x8b},std::byte{0x84},std::byte{0xc7},
        std::byte{0xc0},std::byte{0xe2},std::byte{0x05},std::byte{0x01}};
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
    build_bindings::Cup cup;
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
    build_bindings::SongSelect song_select;
    build_bindings::SortContract sort_contract;
    build_bindings::StepupContract stepup_contract;
    build_bindings::SurfaceContract surface_contract;
};
extern const GameBindings august_bindings;
extern const GameBindings september_bindings;
inline const GameBindings& game_bindings(ImageRange image) noexcept { return *image.bindings; }
}
