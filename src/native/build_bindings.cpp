#include "build_bindings.hpp"
namespace point_zero::native {
const GameBindings august_bindings{};
const GameBindings september_bindings = [] {
    GameBindings result{};
    result.asset_contract.load_rva = 0x3506e0;
    result.asset_contract.release_rva = 0x34f520;
    result.asset_contract.manager_rva = 0xb8b5e90;
    result.boot_text.renderer_rva = 0x5ea1b0;
    result.boot_text.capture_rva = 0x93e369;
    result.browsing.lookup_rva = 0x86db70;
    result.browsing.bank_sort_rva = 0x86faa0;
    result.browsing.chart_difficulty_rva = 0x5f8840;
    result.browsing.style_rva = 0x9482b0;
    result.browsing.options_rva = 0x8841e0;
    result.browsing.battle_rva = 0x8968f0;
    result.browsing.keypad_rva = 0x7ffbb0;
    result.browsing.modal_rva = 0x805ca0;
    result.browsing.list_state_rva = 0x7fa1f0;
    result.browsing.reset_list_rva = 0x7fa1a0;
    result.catalog_contract.catalog_getters_rva = 0x950d10;
    result.catalog_contract.catalog_load_rva = 0x951960;
    result.category_accessors_contract.pins = {{
    {0x86db70,0x86dbe5},
    {0x86dbf0,0x86dc35},
    {0x86e2b0,0x86e316},
    {0x86e320,0x86e38d},
    {0x86e490,0x86e4cc},
    {0x86e5e0,0x86e61c},
    {0x86eaf0,0x86eb2c},
    {0x86ec40,0x86ec7c},
}};
    result.category_accessors_contract.category_style_rva = 0x86df70;
    result.category_badges_contract.music_owner_rva = 0x884170;
    result.category_badges_contract.event_owner_rva = 0x884140;
    result.category_badges_contract.chart_at_rva = 0x5fb3d0;
    result.category_badges_contract.music_of_rva = 0x5f84e0;
    result.category_badges_contract.weekly_rva = 0x893550;
    result.category_badges_contract.recommend_rva = 0x893190;
    result.category_badges_contract.shop_rva = 0x86eb30;
    result.category_badges_contract.kac_rva = 0x887970;
    result.category_generations.category_manager_rva = 0xaab1b08;
    result.category_generations.scene_vtable_rva = 0xd82768;
    result.category_generations.dp_flag_rva = 0xacd59a4;
    result.category_generations.active_flags_rva = 0xacd59b0;
    result.category_generations.idle_callback_rva = 0x8eced0;
    result.category_generations.filter_restore_caller_rva = 0x5fc865;
    result.category_lifecycle_contract.pins = {{
    {0x86ed30, 0x86eddf},
    {0x6377b0, 0x6378bd},
    {0x638770, 0x6387ff},
    {0x6108c0, 0x61094b},
    {0x602c20, 0x602e49},
    {0x686b60, 0x686c93},
    {0x8e9520, 0x8e9722},
    {0x8e9d90, 0x8ea55b},
    {0x8eaf30, 0x8eb18a},
    {0x8eb490, 0x8eb544},
    {0x7ff9b0, 0x7ff9ff},
    {0x606c70, 0x606d37},
    {0x606ac0, 0x606ace},
}};
    result.category_lifecycle_contract.sites_initialize_caller_rva = 0x605ebb;
    result.category_lifecycle_contract.sites_scene_entry_caller_rva = 0x8ea670;
    result.category_lifecycle_contract.sites_scene_tick_caller_rva = 0x96ff4b;
    result.category_lifecycle_contract.sites_scene_refresh_caller_rva = 0x8eb111;
    result.category_lifecycle_contract.sites_preview_caller_rva = 0x6077ba;
    result.category_lifecycle_contract.sites_restore_caller_rva = 0x8ea430;
    result.category_lifecycle_contract.sites_style_apply_caller_rva = 0x8eb501;
    result.category_lifecycle_contract.sites_style_restore_caller_rva = 0x8eb52f;
    result.category_lifecycle_contract.scene_vtable_rva = 0xd82768;
    result.category_lifecycle_contract.idle_callback_rva = 0x8eced0;
    result.category_metadata_contract.game_rva = 0x86dc40;
    result.category_navigation.category_manager_rva = 0xaab1b08;
    result.category_navigation_contract.open_rva = 0x602640;
    result.category_navigation_contract.close_rva = 0x602550;
    result.category_navigation_contract.sound_rva = 0x5c49c0;
    result.category_navigation_contract.exact_rva = 0x5fc260;
    result.category_navigation_contract.restore_chart_rva = 0x5f6920;
    result.category_navigation_contract.set_difficulty_rva = 0x5fc910;
    result.category_navigation_contract.cycle_slot = 0xd51260;
    result.category_navigation_contract.sites_cycle_caller_rva = 0x602ffd;
    result.category_navigation_contract.sites_back_rva = 0x60325e;
    result.category_navigation_contract.sites_back_continuation_rva = 0x6038df;
    result.category_request_contract.settings_rva = 0x884180;
    result.category_request_contract.persistent_rva = 0x86e6d0;
    result.category_request_contract.store_rva = 0x881fd0;
    result.category_request_contract.save_rva = 0x6e62b0;
    result.category_request_contract.clear_rva = 0x6e7ba0;
    result.category_request_contract.group_rva = 0x639830;
    result.category_request_contract.sound_rva = 0x5c49c0;
    result.category_request_sites_contract.pins = {{
    {0x607fc0, 0x6081d8},
    {0x6081e0, 0x6085fd},
    {0x608600, 0x6088bb},
    {0x606510, 0x606a00},
    {0x881fc0, 0x881fc8},
    {0x8eb190, 0x8eb482},
}};
    result.category_request_sites_contract.captures = {0x608018, 0x608392, 0x60870a};
    result.category_request_sites_contract.cancels = {0x60818e, 0x6084af, 0x608871};
    result.category_request_sites_contract.callers = {0x6065ec, 0x6065cc, 0x6065dc};
    result.category_request_sites_contract.category_manager_rva = 0xaab1b08;
    result.category_restore_contract.current_rva = 0x602930;
    result.category_restore_contract.restore_rva = 0x606c70;
    result.category_roots.category_manager_rva = 0xaab1b08;
    result.category_roots.category_storage_rva = 0xaab1b10;
    result.category_storage_contract.construct_rva = 0x5fa950;
    result.category_storage_contract.initialize_rva = 0x5fb810;
    result.category_storage_contract.assign_string_rva = 0x365840;
    result.category_storage_contract.insert_rva = 0x86ceb0;
    result.category_storage_contract.destroy_rva = 0x881910;
    result.category_storage_contract.auxiliary_vtable_rva = 0xcaad80;
    result.category_summary_contract.category_summary_rva = 0x86f1f0;
    result.category_summary_contract.summary_capture_rva = 0x86f257;
    result.category_summary_contract.summary_continuation_rva = 0x86f407;
    result.category_vector_contract.grow_rva = 0x6374f0;
    result.category_vector_contract.destroy_rva = 0x610860;
    result.cup.layer_vtable = 0xdc66c0;
    result.cup.animation_clock_rva = 0xabad550;
    result.difficulty_sort.store_rva = 0xaab1b08;
    result.difficulty_sort.style_flag_rva = 0xaab16b0;
    result.difficulty_sort.visible_bars_rva = 0x1075540;
    result.difficulty_sort.active_flags_rva = 0xacd59b0;
    result.difficulty_sort.dp_flag_rva = 0xacd59a4;
    result.difficulty_sort.sort_return_rva = 0x86fcc8;
    result.filter_contract.code_pins = {{
    {0x6e7d50,131},
    {0x6e7b80,32},
    {0x892ce0,506},
}};
    result.filter_contract.filter_apply_rva = 0x6e7d50;
    result.filter_contract.filter_level_rva = 0x6e7b80;
    result.filter_contract.filter_candidate_rva = 0x892ce0;
    result.filter_contract.filter_apply_caller_rva = 0x892dba;
    result.filter_contract.sites_level_caller_rva = 0x6e7da3;
    result.filter_contract.mode_rva = 0x948100;
    result.filter_contract.rebuild_rva = 0x5fc470;
    result.game_driver.graphics_started_rva = 0xb2a7000;
    result.game_driver.catalog_mutex_rva = 0xb959690;
    result.guide.builder = {0x6820e0, 1632, nullptr};
    result.guide.capture = {0x682e20, 6,
    "2c96a8932afbbb93e85faec5ddbd8df4259e98c6ef97e35f91a890c503c26b48"};
    result.guide.copy_string = {0x3683a0, 205, nullptr};
    result.guide.grow_vector = {0x412510, 549, nullptr};
    result.guide.guide_build_caller_rva = 0x682b84;
    result.guide.guide_create_call_rva = 0x682e26;
    result.level_population.category_manager_rva = 0xaab1b08;
    result.list.pool_rva = 0xb3dd3e0;
    result.list.background_return_rva = 0x5f6ba2;
    result.list.p1_return_rva = 0x5f739c;
    result.list.p2_return_rva = 0x5f73eb;
    result.list.solo_return_rva = 0x5f74ad;
    result.list_chart.p1_site = 0x5f739c;
    result.list_chart.p2_site = 0x5f73eb;
    result.list_chart.solo_site = 0x5f74ad;
    result.list_contract.chart_player_count_rva = 0x9481c0;
    result.list_contract.chart_style_rva = 0x948270;
    result.list_contract.chart_active_player_rva = 0x947f70;
    result.list_contract.emit_rva = 0x34a0a0;
    result.list_hook_contract.background_return_rva = 0x5f6ba2;
    result.list_hook_contract.p1_return_rva = 0x5f739c;
    result.list_hook_contract.p2_return_rva = 0x5f73eb;
    result.list_hook_contract.solo_return_rva = 0x5f74ad;
    result.list_title_contract.title = {0x5f7800, 874};
    result.list_title_contract.title_background_return_rva = 0x5f6bba;
    result.list_title_contract.title_width_end_rva = 0x5f7af8;
    result.list_title_contract.title_entry_rva = 0x5f7800;
    result.original_filter.filter_candidate_state_rva = 0xab9ce78;
    result.original_filter.filter_apply_caller_rva = 0x892dba;
    result.original_filter.category_manager_rva = 0xaab1b08;
    result.original_filter.filter_state_end_rva = 0xaab1ca0;
    result.original_filter.list_vtable_rva = 0xd51258;
    result.original_filter.active_filter_rva = 0xaab1c90;
    result.original_filter.saved_filter_rva = 0xaab1c98;
    result.original_filter.filter_restore_caller_rva = 0x5fc865;
    result.original_filter_state.saved_filter_rva = 0xaab1c98;
    result.original_filter_state.category_manager_rva = 0xaab1b08;
    result.original_filter_state.list_vtable_rva = 0xd51258;
    result.original_filter_state.active_filter_rva = 0xaab1c90;
    result.original_filter_state.filter_vtable_rva = 0xcbf470;
    result.played_calls_contract.relative_import_rva = 0xc8fb88;
    result.played_calls_contract.property_import_rva = 0xc8fbc8;
    result.played_calls_contract.chart_style_rva = 0x948270;
    result.played_chart.active_flags = 0xacd59b0;
    result.played_chart.dp_flag = 0xacd59a4;
    result.played_chart.published_music = 0xacd59d0;
    result.played_chart.published_difficulties = 0xacd59a8;
    result.played_chart.decide_first = 0x600455;
    result.played_chart.decide_second = 0x600489;
    result.played_chart.hud_site = 0x90333d;
    result.played_chart.result_site = 0x91b2ad;
    result.played_chart.stepup_site = 0x600c8e;
    result.played_chart.resource_slots = 0xb8b5e90;
    result.played_contract.pins = {{
    {0x33f0d0, 171},
    {0x600350, 1931},
    {0x9031b0, 478},
    {0x91ae60, 1269},
    {0x833250, 50},
    {0x9246c0, 139},
}};
    result.played_contract.confirmation_first_rva = 0x600455;
    result.played_contract.confirmation_second_rva = 0x600489;
    result.played_contract.hud_capture_rva = 0x90333d;
    result.played_contract.result_capture_rva = 0x91b2ad;
    result.played_contract.confirmation_first_return_rva = 0x600465;
    result.played_contract.confirmation_second_return_rva = 0x600499;
    result.played_contract.hud_return_rva = 0x903350;
    result.played_contract.named_return_rva = 0x83327c;
    result.played_contract.notes_capture_rva = 0x9246da;
    result.played_contract.result_return_first_rva = 0x91b2e5;
    result.played_contract.result_return_second_rva = 0x91b2fa;
    result.song_select.category_data = 0xaab1b08;
    result.song_select.category_list = 0xd51258;
    result.song_select.groups = {
    0xcb4d68, 0xcb4af0, 0xcb5090, 0xcb4ba0, 0xcb4c50, 0xcb4458,
    0xcb43b0, 0, 0xcb49b0, 0xcb4e40, 0xcb4a70, 0xcae6e0};
    result.sort_contract.mode_rva = 0x948100;
    result.sort_contract.level_rva = 0x86ff90;
    result.sort_contract.sort_return_rva = 0x86fcc8;
    result.stepup_contract.builder = {0x600bc0, 522};
    result.stepup_contract.capture_rva = 0x600c8e;
    result.stepup_contract.named_return_rva = 0x600c9b;
    result.surface_contract.cup_rva = 0x5fef10;
    result.surface_contract.bitmap_import = 0xc8fbd8;
    result.captures.confirmation[5] = std::byte{0xbf};
    result.captures.stepup[5] = std::byte{0xc2};
    return result;
}();
}
