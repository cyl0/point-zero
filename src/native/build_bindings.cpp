#include "build_bindings.hpp"
namespace point_zero::native {
const GameBindings august_bindings{};
const GameBindings september_bindings = [] {
    GameBindings result{};
    result.asset_contract.game_code = {{
    {0x3506e0, 611},
    {0x34eeb0, 661},
    {0x34f150, 963},
    {0x34f520, 193},
    {0x353640, 220},
}};
    result.asset_contract.utils_imports = {{
    {0xc8fd18, 35, 0xAF50}, {0xc8fcf8, 55, 0x127F0},
    {0xc8fd00, 59, 0x11240}, {0xc8fd20, 62, 0xBA60}, {0xc8fd28, 66, 0xCFD0},
}};
    result.asset_contract.avs_imports = {{
    {0xc8ffe0, 16}, {0xc8ffe8, 17},
    {0xc8ff90, 381}, {0xc8fd90, 258},
    {0xc8fdb0, 76}, {0xc8ff00, 77},
}};
    result.asset_contract.literals = {{
    {0xe6a33c, "%s"}, {0xe6a374, "%s%s"},
    {0xe6a380, "/afp%d%s"}, {0xe6a3c8, "imagefs"},
}};
    result.asset_contract.manager_rva = 0xb8b5e90;
    result.avs_read.read_import_rva = 0xc8fd88;
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
    result.catalog_contract.header_rva = 0xacd6900;
    result.catalog_contract.index_rva = 0xacd6910;
    result.catalog_contract.read_rva = 0x9519c7;
    result.catalog_contract.import_rva = 0xc8fd88;
    result.catalog_contract.return_rva = 0x9519cd;
    result.catalog_contract.pins = {
    build_bindings::CatalogContract::Pin{0x950b50,47},
    build_bindings::CatalogContract::Pin{0x950b80,61},
    build_bindings::CatalogContract::Pin{0x950bc0,59},
    build_bindings::CatalogContract::Pin{0x950d10,8},
    build_bindings::CatalogContract::Pin{0x950ff0,34},
    build_bindings::CatalogContract::Pin{0x82cce0,133},
    build_bindings::CatalogContract::Pin{0x34f520,193},
    build_bindings::CatalogContract::Pin{0x33d650,425},
    build_bindings::CatalogContract::Pin{0x93b9f0,419},
    build_bindings::CatalogContract::Pin{0x93f3a0,684},
    build_bindings::CatalogContract::Pin{0x951960,214},
    build_bindings::CatalogContract::Pin{0x951af0,31}
};
    result.catalog_contract.omni_header_patch_1_rva = 0x950b68;
    result.catalog_contract.omni_header_patch_2_rva = 0x950ba5;
    result.catalog_contract.omni_header_patch_3_rva = 0x950bd8;
    result.catalog_contract.omni_index_patch_rva = 0x950d11;
    result.catalog_contract.capacity_immediate_rva = 0x9519ab;
    result.catalog_contract.capacity_branch_rva = 0x951a04;
    result.catalog_contract.catalog_getters_rva = 0x950d10;
    result.catalog_contract.catalog_load_rva = 0x951960;
    result.category_accessors_contract.pins = {{
    {0x86db70,0x86dbe5,
     0x40AB0,0x40B67,0xFE530,5,15,
     {0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20}},
    {0x86dbf0,0x86dc35,
     0x40B70,0x40C52,0xFE160,6,0,
     {0x40,0x53,0x48,0x83,0xEC,0x20}},
    {0x86e2b0,0x86e316,
     0x3FC00,0x3FCDE,0xFE2F0,5,15,
     {0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18}},
    {0x86e320,0x86e38d,
     0,0,0,5,0,{0x48,0x89,0x5C,0x24,0x08}},
    {0x86e490,0x86e4cc,
     0x429A0,0x42A98,0xFDF50,5,16,
     {0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x48,0x63,0xDA,0x48,0x8B,0xF9}},
    {0x86e5e0,0x86e61c,
     0x427A0,0x42898,0xFE7F0,5,16,
     {0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x48,0x63,0xDA,0x48,0x8B,0xF9}},
    {0x86eaf0,0x86eb2c,
     0x428A0,0x42998,0xFE000,5,16,
     {0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x48,0x63,0xDA,0x48,0x8B,0xF9}},
    {0x86ec40,0x86ec7c,
     0x42680,0x42778,0xFE0B0,5,16,
     {0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x48,0x63,0xDA,0x48,0x8B,0xF9}},
}};
    result.category_accessors_contract.category_accessor_rva = 0x86dbf0;
    result.category_accessors_contract.category_style_rva = 0x86df70;
    result.category_badges_contract.pins = {{
    {0x884170,0x884178},
    {0x884140,0x884148},
    {0x5fb3d0,0x5fb454},
    {0x5f84e0,0x5f84e4},
    {0x893550,0x8935a8},
    {0x893190,0x8931db},
    {0x86eb30,0x86ec39},
    {0x887970,0x8879cc},
    {0x5fdc40,0x5fdc45},
    {0x5fdc70,0x5fdc73},
    {0x975f20,0x975f2f},
    {0x881ed0,0x881ed8},
    {0x85b2d0,0x85b2f7},
    {0x8879e0,0x887ac4},
    {0x887ad0,0x887b3c},
    {0x86e460,0x86e484},
    {0x86e6d0,0x86e6dd},
}};
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
    result.category_generations.list_vtable_rva = 0xd51258;
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
    result.category_lifecycle_contract.support_pins = {{
    {0x605e70, 0x6063f3},
    {0x86d050, 0x86d53b},
    {0x8ea560, 0x8eabef},
    {0x96fea0, 0x96ff5e},
    {0x8eced0, 0x8ecf5c},
    {0x9482b0, 0x9482c7},
    {0x8841e0, 0x8841e8},
    {0x8968f0, 0x896904},
    {0x9480e0, 0x9480e7},
    {0x607590, 0x607b0b},
    {0x7ffbb0, 0x7ffc0a},
    {0x7ffc40, 0x7ffca2},
    {0xa78ea0, 0xa79027},
    {0x805ca0, 0x805cf1},
    {0x805ba0, 0x805bcc},
    {0x805bd0, 0x805bfc},
    {0x805c00, 0x805c2c},
    {0x805c80, 0x805c92},
    {0x948120, 0x94813e},
    {0x604260, 0x604404},
    {0x86d980, 0x86da38},
    {0x86df70, 0x86df85},
    {0x7fa1f0, 0x7fa1f8},
    {0x7fa1a0, 0x7fa1b1},
}};
    result.category_lifecycle_contract.category_lookup_rva = 0x86d980;
    result.category_lifecycle_contract.category_state_rva = 0xaab1aa8;
    result.category_lifecycle_contract.dp_flag_rva = 0xacd59a4;
    result.category_lifecycle_contract.active_flags_rva = 0xacd59b0;
    result.category_lifecycle_contract.controller_slot_1_rva = 0xa7e76d8;
    result.category_lifecycle_contract.controller_slot_2_rva = 0xa7e7758;
    result.category_lifecycle_contract.controller_slot_3_rva = 0xa7e7750;
    result.category_lifecycle_contract.scene_slot_1_rva = 0xb28aad8;
    result.category_lifecycle_contract.scene_slot_2_rva = 0xb298250;
    result.category_lifecycle_contract.sites_initialize_caller_rva = 0x605ebb;
    result.category_lifecycle_contract.scene_entry_slot_rva = 0xd827d0;
    result.category_lifecycle_contract.scene_refresh_slot_rva = 0xd827e0;
    result.category_lifecycle_contract.scene_entry_rva = 0x8ea560;
    result.category_lifecycle_contract.scene_refresh_rva = 0x8eaf30;
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
    result.category_navigation.list_vtable_rva = 0xd51258;
    result.category_navigation.level_group_vtable_rva = 0xcb4af0;
    result.category_navigation_contract.pins = {{
    {0x602640, 0x602752},
    {0x639680, 0x63973c},
    {0x602e50, 0x6039a6},
    {0x593760, 0x5937ee},
    {0x602550, 0x602634},
    {0x5c49c0, 0x5c49fb},
    {0x5fc260, 0x5fc346},
    {0x5f6920, 0x5f692d},
    {0x5fc910, 0x5fc9a8},
}};
    result.category_navigation_contract.cycle_slot = 0xd51260;
    result.category_navigation_contract.sites_cycle_caller_rva = 0x602ffd;
    result.category_navigation_contract.sites_back_rva = 0x60325e;
    result.category_navigation_contract.back_capture_end_rva = 0x603263;
    result.category_navigation_contract.sites_back_continuation_rva = 0x6038df;
    result.category_request_contract.pins = {{
    {0x884180, 0x884188},
    {0x86e6d0, 0x86e6dd},
    {0x881fd0, 0x881fd8},
    {0x6e62b0, 0x6e638a},
    {0x6e7ba0, 0x6e7bec},
    {0x639830, 0x639939},
    {0x5c49c0, 0x5c49fb},
}};
    result.category_request_contract.direct_targets = {
    0x82ce30, 0x5fb460, 0x5fca90, 0x5fa640, 0x5f9980, 0x5fcc00,
    0x5fd840, 0xb1ab94, 0x5fcf70, 0x6383f0, 0xb76174, 0x605310,
    0x605330, 0x5c4230, 0x5c5300, 0x5c3f50, 0xa8f910,
};
    result.category_request_contract.singleton_slots = {0xaab1c90, 0xaab1c98};
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
    result.category_request_sites_contract.foreign_sites = {0x8eb438, 0x608636};
    result.category_request_sites_contract.foreign_targets = {0x7ffbb0, 0x7d4e20};
    result.category_request_sites_contract.category_manager_rva = 0xaab1b08;
    result.category_restore_contract.pins = {{
    {0x602930, 0x6029da},
    {0x606c70, 0x606d37},
    {0x604df0, 0x605128},
    {0x5fc350, 0x5fc465},
    {0x606da0, 0x6071db},
    {0x6071e0, 0x607583},
    {0x607590, 0x607b0b},
    {0x607b10, 0x607fa9},
}};
    result.category_restore_contract.current_rva = 0x602930;
    result.category_restore_contract.restore_rva = 0x606c70;
    result.category_roots.list_vtable_rva = 0xd51258;
    result.category_roots.level_group_vtable_rva = 0xcb4af0;
    result.category_roots.category_manager_rva = 0xaab1b08;
    result.category_roots.category_storage_rva = 0xaab1b10;
    result.category_storage_contract.call_entries = {
    0x5fa950, 0x5fb810, 0x365840, 0x86ceb0, 0x881910};
    result.category_storage_contract.auxiliary_bindings = {
    0xe93e28, 0x5fad50, 0x683400, 0x5fbf60, 0x5fbfe0, 0x5faf10, 0x5fbe90};
    result.category_storage_contract.auxiliary_table_rva = 0xcaad78;
    result.category_storage_contract.auxiliary_col_rva = 0xe93e28;
    result.category_storage_contract.construct_rva = 0x5fa950;
    result.category_storage_contract.initialize_rva = 0x5fb810;
    result.category_storage_contract.assign_string_rva = 0x365840;
    result.category_storage_contract.insert_rva = 0x86ceb0;
    result.category_storage_contract.destroy_rva = 0x881910;
    result.category_storage_contract.auxiliary_vtable_rva = 0xcaad80;
    result.category_summary_contract.pins = {{
    {0x86f1f0, 0x86f42c},
    {0x5fc910, 0x5fc9a8},
    {0x5fb3d0, 0x5fb454},
    {0x5f8840, 0x5f895e},
    {0x5f84e0, 0x5f84e4},
    {0x5f84f0, 0x5f84f8},
    {0x5fdc40, 0x5fdc45},
    {0x5fdc70, 0x5fdc73},
    {0x881fc0, 0x881fc8},
    {0x884170, 0x884178},
    {0x975f20, 0x975f2f},
    {0xb1a990, 0xb1a9b1},
    // 870546 MOVDQA [RIP+4E2DD2h]: four int32 7 seeds; the last two are
    // immediate 7s stored on the stack by the admitted calculation body.
    {0xd51300, 0xd51310, RangeUse::image_read},
}};
    result.category_summary_contract.category_summary_rva = 0x86f1f0;
    result.category_summary_contract.summary_capture_rva = 0x86f257;
    result.category_summary_contract.summary_continuation_rva = 0x86f407;
    result.category_vector_contract.entries = {0x6374f0, 0x610860};
    result.category_vector_contract.grow_rva = 0x6374f0;
    result.category_vector_contract.destroy_rva = 0x610860;
    result.cup.layer_vtable = 0xdc66c0;
    result.cup.animation_clock_rva = 0xabad550;
    result.difficulty_sort.store_rva = 0xaab1b08;
    result.difficulty_sort.list_vtable = 0xd51258;
    result.difficulty_sort.style_flag_rva = 0xaab16b0;
    result.difficulty_sort.visible_bars_rva = 0x1075540;
    result.difficulty_sort.active_flags_rva = 0xacd59b0;
    result.difficulty_sort.dp_flag_rva = 0xacd59a4;
    result.difficulty_sort.folder_group_vtable_rva = 0xcb4458;
    result.difficulty_sort.sort_return_rva = 0x86fcc8;
    result.filter_contract.code_pins = {{
    {0x6e7d50,131},
    {0x6e7b80,32},
    {0x892ce0,506},
    {0x948100,7},
    {0x884180,8},
    {0x881fd0,8},
    {0x68a4c0,541},
    {0x82cce0,160},
    {0x5fc470,1172},
}};
    result.filter_contract.candidate_table = {0xd55fa8,24};
    result.filter_contract.level_bindings = {
    0xe990d0,0x68a9e0,0x6e7b80,0x63d700,0x68ab20,0x68ab30,0x68ad50,0x68ad60};
    result.filter_contract.control_bindings = {0xe991d8,0x68ad90,0x68ad70};
    result.filter_contract.filter_apply_rva = 0x6e7d50;
    result.filter_contract.filter_level_rva = 0x6e7b80;
    result.filter_contract.filter_candidate_rva = 0x892ce0;
    result.filter_contract.level_table_rva = 0xcbf468;
    result.filter_contract.control_table_rva = 0xcbf518;
    result.filter_contract.filter_apply_caller_rva = 0x892dba;
    result.filter_contract.sites_level_caller_rva = 0x6e7da3;
    result.filter_contract.mode_rva = 0x948100;
    result.filter_contract.rebuild_rva = 0x5fc470;
    result.folder_contract.pins = {{
    {0x5f3900, 3784},
    {0x5fddb0, 787},
    {0x33f790, 356},
    {0x34a680, 545},
}};
    result.game_driver.graphics_started_rva = 0xb2a7000;
    result.game_driver.catalog_mutex_rva = 0xb959690;
    result.guide.builder = {0x6820e0, 1632, nullptr};
    result.guide.factory = {0x349cf0, 932, nullptr};
    result.guide.renderer = {0x682a80, 1718, nullptr};
    result.guide.capture = {0x682e20, 6,
    "2c96a8932afbbb93e85faec5ddbd8df4259e98c6ef97e35f91a890c503c26b48"};
    result.guide.copy_string = {0x3683a0, 205, nullptr};
    result.guide.grow_vector = {0x412510, 549, nullptr};
    result.guide.guide_build_caller_rva = 0x682b84;
    result.guide.guide_create_call_rva = 0x682e26;
    result.level_population.category_manager_rva = 0xaab1b08;
    result.level_population.list_vtable_rva = 0xd51258;
    result.list.pool_rva = 0xb3dd3e0;
    result.list.sprite_vtable = 0xe6a810;
    result.list.background_return_rva = 0x5f6ba2;
    result.list.p1_return_rva = 0x5f739c;
    result.list.p2_return_rva = 0x5f73eb;
    result.list.solo_return_rva = 0x5f74ad;
    result.list_chart.p1_site = 0x5f739c;
    result.list_chart.p2_site = 0x5f73eb;
    result.list_chart.solo_site = 0x5f74ad;
    result.list_chart.normal_names = 0x105b0f0;
    result.list_chart.compact_names = 0x105b300;
    result.list_contract.game_code = {{
    {0x9481c0, 55},
    {0x948270, 57},
    {0x947f70, 42},
    {0x34a0a0, 856},
    {0x34b6d0, 186},
    {0x34af80, 19},
    {0x3527d0, 82},
    {0x34ae30, 112},
    {0x34b0f0, 156},
    {0x34b190, 119},
    {0x34b460, 27},
}};
    result.list_contract.game_tables = {{
    {0xe6a838, 0x34af80}, // visible, vtable +28
    {0xe6a860, 0x34b0f0}, // emit's +50 matrix initialization
    {0xe6a878, 0x34b190}, // emit's +68 pivot
    {0xe6a898, 0x34b460}, // wrappers compare the +88 flag method
    {0xe6a8e0, 0x34b6d0}, // emit's +D0 scale
    {0xe6a900, 0x34b810}, // initializer compares +F0 before inlining it
}};
    result.list_contract.game_imports = {{
    {0xc8fb50,56}, {0xc8fb88,106}, {0xc8fbc8,116},
    {0xc8fc40,35}, {0xc8fc48,37}, {0xc8fc68,45},
    {0xc8fc80,47}, {0xc8fc88,51}, {0xc8fc90,49},
    {0xc8fc98,50}, {0xc8fca0,46}, {0xc8fcb0,52}, {0xc8fcb8,85},
}};
    result.list_contract.chart_player_count_rva = 0x9481c0;
    result.list_contract.chart_style_rva = 0x948270;
    result.list_contract.chart_active_player_rva = 0x947f70;
    result.list_contract.emit_rva = 0x34a0a0;
    result.list_contract.visible_rva = 0x34af80;
    result.list_hook_contract.pins = {{
    {0x349990, 856},
    {0x5f6930, 997},
    {0x5f72b0, 525},
}};
    result.list_hook_contract.background_return_rva = 0x5f6ba2;
    result.list_hook_contract.p1_return_rva = 0x5f739c;
    result.list_hook_contract.p2_return_rva = 0x5f73eb;
    result.list_hook_contract.solo_return_rva = 0x5f74ad;
    result.list_title_contract.title = {0x5f7800, 874};
    result.list_title_contract.background = {0x5f6930, 997};
    result.list_title_contract.width_rva = 0xcaa188;
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
    result.played.layer_vtable = 0xdc66c0;
    result.played_calls_contract.game_code = {{
    {0x948270, 57},
}};
    result.played_calls_contract.game_imports = {{
    {0xc8fb88, 106, 0x1DA50}, {0xc8fbc8, 116, 0x22D60},
}};
    result.played_calls_contract.chart_style_rva = 0x948270;
    result.played_chart.active_flags = 0xacd59b0;
    result.played_chart.dp_flag = 0xacd59a4;
    result.played_chart.published_music = 0xacd59d0;
    result.played_chart.published_difficulties = 0xacd59a8;
    result.played_chart.layer_vtable = 0xdc66c0;
    result.played_chart.decide_first = 0x600455;
    result.played_chart.decide_second = 0x600489;
    result.played_chart.hud_site = 0x90333d;
    result.played_chart.result_site = 0x91b2ad;
    result.played_chart.stepup_site = 0x600c8e;
    result.played_chart.stepup_vtable = 0xd7f898;
    result.played_chart.stepup_callback = 0x8d8280;
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
    result.result_notes.layer_vtable = 0xdc66c0;
    result.result_plate.layer_vtable = 0xdc66c0;
    result.song_select.category_data = 0xaab1b08;
    result.song_select.category_list = 0xd51258;
    result.song_select.groups = {
    0xcb4d68, 0xcb4af0, 0xcb5090, 0xcb4ba0, 0xcb4c50, 0xcb4458,
    0xcb43b0, 0, 0xcb49b0, 0xcb4e40, 0xcb4a70, 0xcae6e0};
    result.sort_contract.pins = {{
    {0x86faa0, 668},
    {0xb76410, 820},
    {0xb76310, 204},
    {0x86ff90, 252},
    {0x870350, 447},
    {0x950ff0, 33},
    {0x948100, 7},
    {0x82cce0, 160},
    {0x5f84e0, 4},
    {0x950de0, 164},
    {0xb7d1fc, 71},
    {0x948270, 57},
    // Immediate bar payload/type accessors; never infer visible from contents.
    {0x5fdc40, 5},
    {0x5fdc70, 3},
    // Direct qsort validation/cookie helpers and its unwind handler.
    {0xb79020, 32},
    {0xb76154, 30},
    {0xb1a990, 33},
    {0xb1a880, 29},
}};
    result.sort_contract.qsort_rva = 0xb76410;
    result.sort_contract.dispatcher_call_rva = 0x86fcc3;
    result.sort_contract.comparator_selection_rva = 0x86fb77;
    result.sort_contract.mode_rva = 0x948100;
    result.sort_contract.level_rva = 0x86ff90;
    result.sort_contract.sort_return_rva = 0x86fcc8;
    result.stepup_contract.pins = {{
    {0x600bc0, 522},
    {0x341e20, 41},
    {0x8d3c20, 338},
    {0x8d8280, 816},
    {0x6018f0, 91},
}};
    result.stepup_contract.capture_rva = 0x600c8e;
    result.stepup_contract.named_return_rva = 0x600c9b;
    result.surface_contract.cup = {0x5fef10, 1467};
    result.surface_contract.bitmap_import = 0xc8fbd8;
    result.catalog_contract.read_call = {std::byte{0xff},std::byte{0x15},std::byte{0xbb},std::byte{0xe3},std::byte{0x33},std::byte{0}};
    result.captures.confirmation[5] = std::byte{0xbf};
    result.captures.stepup[5] = std::byte{0xc2};
    return result;
}();
}
