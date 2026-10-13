#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include "memory.hpp"
namespace point_zero::native {
namespace build_bindings {
struct AssetContract {
struct Binding { std::uint32_t slot; std::uint16_t ordinal; std::uint32_t target{}; };
struct Literal { std::uint32_t rva; std::string_view text; };
struct Pin { std::uint32_t rva, size; };
    std::array<Pin, 5> game_code{{
    {0x3519A0, 611},
    {0x350170, 661},
    {0x350410, 963},
    {0x3507E0, 193},
    {0x354900, 220},
}};
    std::array<Binding, 5> utils_imports{{
    {0xC91D18, 35, 0xAF50}, {0xC91CF8, 55, 0x127F0},
    {0xC91D00, 59, 0x11240}, {0xC91D20, 62, 0xBA60}, {0xC91D28, 66, 0xCFD0},
}};
    std::array<Binding, 6> avs_imports{{
    {0xC91FE0, 16}, {0xC91FE8, 17},
    {0xC91F90, 381}, {0xC91D90, 258},
    {0xC91DB0, 76}, {0xC91F00, 77},
}};
    std::array<Literal, 4> literals{{
    {0xE6C33C, "%s"}, {0xE6C374, "%s%s"},
    {0xE6C380, "/afp%d%s"}, {0xE6C3C8, "imagefs"},
}};
    std::uint32_t manager_rva = 0xB8B7E90;
};
struct AvsRead {
    std::uint32_t read_import_rva = 0xc91d88;
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
struct Pin { std::uintptr_t rva; std::size_t size; };
    std::array<Pin, 12> pins{
    Pin{0x951e10,47},
    Pin{0x951e40,61},
    Pin{0x951e80,59},
    Pin{0x951fd0,8},
    Pin{0x9522b0,34},
    Pin{0x82dfa0,133},
    Pin{0x3507e0,193},
    Pin{0x33e910,425},
    Pin{0x93ccb0,419},
    Pin{0x940660,684},
    Pin{0x952c20,214},
    Pin{0x952db0,31}
};
    std::uint32_t catalog_getters_rva = 0x951fd0;
    std::uint32_t catalog_load_rva = 0x952c20;
};
struct CategoryAccessorsContract {
struct Pin {
    std::uint32_t begin, end;
    std::uint32_t pl_begin, pl_end;
    std::uint32_t object;
    unsigned near_stolen, far_stolen;
    std::array<unsigned char,16> original;
};
    std::array<Pin, 8> pins{{
    {0x86EE30,0x86EEA5,
     0x40AB0,0x40B67,0xFE530,5,15,
     {0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20}},
    {0x86EEB0,0x86EEF5,
     0x40B70,0x40C52,0xFE160,6,0,
     {0x40,0x53,0x48,0x83,0xEC,0x20}},
    {0x86F570,0x86F5D6,
     0x3FC00,0x3FCDE,0xFE2F0,5,15,
     {0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18}},
    {0x86F5E0,0x86F64D,
     0,0,0,5,0,{0x48,0x89,0x5C,0x24,0x08}},
    {0x86F750,0x86F78C,
     0x429A0,0x42A98,0xFDF50,5,16,
     {0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x48,0x63,0xDA,0x48,0x8B,0xF9}},
    {0x86F8A0,0x86F8DC,
     0x427A0,0x42898,0xFE7F0,5,16,
     {0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x48,0x63,0xDA,0x48,0x8B,0xF9}},
    {0x86FDB0,0x86FDEC,
     0x428A0,0x42998,0xFE000,5,16,
     {0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x48,0x63,0xDA,0x48,0x8B,0xF9}},
    {0x86FF00,0x86FF3C,
     0x42680,0x42778,0xFE0B0,5,16,
     {0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x48,0x63,0xDA,0x48,0x8B,0xF9}},
}};
    std::uint32_t category_accessor_rva = 0x86EEB0;
    std::uint32_t category_style_rva = 0x86F230;
};
struct CategoryBadgesContract {
struct Pin { std::uint32_t begin, end; };
    std::array<Pin, 17> pins{{
    {0x885430,0x885438},
    {0x885400,0x885408},
    {0x5FC690,0x5FC714},
    {0x5F97A0,0x5F97A4},
    {0x894810,0x894868},
    {0x894450,0x89449B},
    {0x86FDF0,0x86FEF9},
    {0x888C30,0x888C8C},
    {0x5FEF00,0x5FEF05},
    {0x5FEF30,0x5FEF33},
    {0x9771E0,0x9771EF},
    {0x883190,0x883198},
    {0x85C590,0x85C5B7},
    {0x888CA0,0x888D84},
    {0x888D90,0x888DFC},
    {0x86F720,0x86F744},
    {0x86F990,0x86F99D},
}};
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
    std::uint32_t list_vtable_rva = 0xD53278;
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
    std::array<Pin, 24> support_pins{{
    {0x607130, 0x6076B3},
    {0x86E310, 0x86E7FB},
    {0x8EB820, 0x8EBEAF},
    {0x971160, 0x97121E},
    {0x8EE190, 0x8EE21C},
    {0x949570, 0x949587},
    {0x8854A0, 0x8854A8},
    {0x897BB0, 0x897BC4},
    {0x9493A0, 0x9493A7},
    {0x608850, 0x608DCB},
    {0x800E70, 0x800ECA},
    {0x800F00, 0x800F62},
    {0xA7A160, 0xA7A2E7},
    {0x806F60, 0x806FB1},
    {0x806E60, 0x806E8C},
    {0x806E90, 0x806EBC},
    {0x806EC0, 0x806EEC},
    {0x806F40, 0x806F52},
    {0x9493E0, 0x9493FE},
    {0x605520, 0x6056C4},
    {0x86EC40, 0x86ECF8},
    {0x86F230, 0x86F245},
    {0x7FB4B0, 0x7FB4B8},
    {0x7FB460, 0x7FB471},
}};
    std::uint32_t category_lookup_rva = 0x86EC40;
    std::uint32_t category_state_rva = 0xAAB3AA8;
    std::uint32_t dp_flag_rva = 0xACD79A4;
    std::uint32_t active_flags_rva = 0xACD79B0;
    std::uint32_t controller_slot_1_rva = 0xA7E96D8;
    std::uint32_t controller_slot_2_rva = 0xA7E9758;
    std::uint32_t controller_slot_3_rva = 0xA7E9750;
    std::uint32_t scene_slot_1_rva = 0xB28CAD8;
    std::uint32_t scene_slot_2_rva = 0xB29A250;
    std::uint32_t sites_initialize_caller_rva = 0x60717B;
    std::uint32_t scene_entry_slot_rva = 0xD847F0;
    std::uint32_t scene_refresh_slot_rva = 0xD84800;
    std::uint32_t scene_entry_rva = 0x8EB820;
    std::uint32_t scene_refresh_rva = 0x8EC1F0;
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
    std::uint32_t pl_rva = 0x3FE10;
};
struct CategoryNavigation {
    std::uint32_t category_manager_rva = 0xAAB3B08;
    std::uint32_t list_vtable_rva = 0xD53278;
    std::uint32_t level_group_vtable_rva = 0xCB6B10;
};
struct CategoryNavigationContract {
struct Pin { std::uint32_t begin, end; };
    std::array<Pin, 9> pins{{
    {0x603900, 0x603A12},
    {0x63A940, 0x63A9FC},
    {0x604110, 0x604C66},
    {0x594A20, 0x594AAE},
    {0x603810, 0x6038F4},
    {0x5C5C80, 0x5C5CBB},
    {0x5FD520, 0x5FD606},
    {0x5F7BE0, 0x5F7BED},
    {0x5FDBD0, 0x5FDC68},
}};
    std::uint32_t cycle_slot = 0xD53280;
    std::uint32_t sites_cycle_caller_rva = 0x6042BD;
    std::uint32_t sites_back_rva = 0x60451E;
    std::uint32_t back_capture_end_rva = 0x604523;
    std::uint32_t sites_back_continuation_rva = 0x604B9F;
};
struct CategoryRequestContract {
struct Pin { std::uint32_t begin, end; };
    std::array<Pin, 7> pins{{
    {0x885440, 0x885448},
    {0x86F990, 0x86F99D},
    {0x883290, 0x883298},
    {0x6E7570, 0x6E764A},
    {0x6E8E60, 0x6E8EAC},
    {0x63AAF0, 0x63ABF9},
    {0x5C5C80, 0x5C5CBB},
}};
    std::array<std::uint32_t, 17> direct_targets{
    0x82E0F0, 0x5FC720, 0x5FDD50, 0x5FB900, 0x5FAC40, 0x5FDEC0,
    0x5FEB00, 0xB1BE54, 0x5FE230, 0x6396B0, 0xB77434, 0x6065D0,
    0x6065F0, 0x5C54F0, 0x5C65C0, 0x5C5210, 0xA90BD0,
};
    std::array<std::uint32_t, 2> singleton_slots{0xAAB3C90, 0xAAB3C98};
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
    std::array<std::uint32_t, 2> foreign_sites{0x8EC6F8, 0x6098F6};
    std::array<std::uint32_t, 2> foreign_targets{0x800E70, 0x7D60E0};
    std::uint32_t category_manager_rva = 0xAAB3B08;
};
struct CategoryRestoreContract {
struct Pin { std::uint32_t begin, end; };
    std::array<Pin, 8> pins{{
    {0x603BF0, 0x603C9A},
    {0x607F30, 0x607FF7},
    {0x6060B0, 0x6063E8},
    {0x5FD610, 0x5FD725},
    {0x608060, 0x60849B},
    {0x6084A0, 0x608843},
    {0x608850, 0x608DCB},
    {0x608DD0, 0x609269},
}};
    std::uint32_t current_rva = 0x603BF0;
    std::uint32_t restore_rva = 0x607F30;
};
struct CategoryRoots {
    std::uint32_t list_vtable_rva = 0xD53278;
    std::uint32_t level_group_vtable_rva = 0xCB6B10;
    std::uint32_t category_manager_rva = 0xAAB3B08;
    std::uint32_t category_storage_rva = 0xAAB3B10;
};
struct CategoryStorageContract {
    std::array<std::uint32_t, 5> call_entries{
    0x5FBC10, 0x5FCAD0, 0x366B00, 0x86E170, 0x882BD0};
    std::array<std::uint32_t, 7> auxiliary_bindings{
    0xE95E28, 0x5FC010, 0x6846C0, 0x5FD220, 0x5FD2A0, 0x5FC1D0, 0x5FD150};
    std::uint32_t auxiliary_table_rva = 0xCACD98;
    std::uint32_t auxiliary_col_rva = 0xE95E28;
    std::uint32_t construct_rva = 0x5FBC10;
    std::uint32_t initialize_rva = 0x5FCAD0;
    std::uint32_t assign_string_rva = 0x366B00;
    std::uint32_t insert_rva = 0x86E170;
    std::uint32_t destroy_rva = 0x882BD0;
    std::uint32_t auxiliary_vtable_rva = 0xCACDA0;
};
struct CategorySummaryContract {
struct Pin {
    std::uint32_t begin, end;
    RangeUse use{RangeUse::image_code};
};
    std::array<Pin, 13> pins{{
    {0x8704B0, 0x8706EC},
    {0x5FDBD0, 0x5FDC68},
    {0x5FC690, 0x5FC714},
    {0x5F9B00, 0x5F9C1E},
    {0x5F97A0, 0x5F97A4},
    {0x5F97B0, 0x5F97B8},
    {0x5FEF00, 0x5FEF05},
    {0x5FEF30, 0x5FEF33},
    {0x883280, 0x883288},
    {0x885430, 0x885438},
    {0x9771E0, 0x9771EF},
    {0xB1BC50, 0xB1BC71},
    // 870546 MOVDQA [RIP+4E2DD2h]: four int32 7 seeds; the last two are
    // immediate 7s stored on the stack by the admitted calculation body.
    {0xD53320, 0xD53330, RangeUse::image_read},
}};
    std::uint32_t category_summary_rva = 0x8704B0;
    std::uint32_t summary_capture_rva = 0x870517;
    std::uint32_t summary_continuation_rva = 0x8706C7;
};
struct CategoryVectorContract {
    std::array<std::uint32_t, 2> entries{0x6387B0, 0x611B20};
    std::uint32_t grow_rva = 0x6387B0;
    std::uint32_t destroy_rva = 0x611B20;
};
struct Cup {
    std::uint32_t layer_vtable = 0xDC86E0;
    std::uint32_t animation_clock_rva = 0xABAF550;
};
struct DifficultySort {
    std::uint32_t store_rva = 0xAAB3B08;
    std::uint32_t list_vtable = 0xD53278;
    std::uint32_t style_flag_rva = 0xAAB36B0;
    std::uint32_t visible_bars_rva = 0x1077540;
    std::uint32_t active_flags_rva = 0xACD79B0;
    std::uint32_t dp_flag_rva = 0xACD79A4;
    std::uint32_t folder_group_vtable_rva = 0xCB6478;
    std::uint32_t sort_return_rva = 0x870F88;
};
struct FilterContract {
struct Pin { std::uint32_t rva, size; };
    std::array<Pin, 9> code_pins{{
    {0x6E9010,131},
    {0x6E8E40,32},
    {0x893FA0,506},
    {0x9493C0,7},
    {0x885440,8},
    {0x883290,8},
    {0x68B780,541},
    {0x82DFA0,160},
    {0x5FD730,1172},
}};
    Pin candidate_table{0xD57FC8,24};
    std::array<std::uint32_t, 8> level_bindings{
    0xE9B0D0,0x68BCA0,0x6E8E40,0x63E9C0,0x68BDE0,0x68BDF0,0x68C010,0x68C020};
    std::array<std::uint32_t, 3> control_bindings{0xE9B1D8,0x68C050,0x68C030};
    std::uint32_t filter_apply_rva = 0x6E9010;
    std::uint32_t filter_level_rva = 0x6E8E40;
    std::uint32_t filter_candidate_rva = 0x893FA0;
    std::uint32_t level_table_rva = 0xCC1488;
    std::uint32_t control_table_rva = 0xCC1538;
    std::uint32_t filter_apply_caller_rva = 0x89407A;
    std::uint32_t sites_level_caller_rva = 0x6E9063;
    std::uint32_t mode_rva = 0x9493C0;
    std::uint32_t rebuild_rva = 0x5FD730;
};
struct FolderContract {
struct Pin { std::uint32_t rva, size;  };
    std::array<Pin, 4> pins{{
    {0x5F4BC0, 3784},
    {0x5FF070, 787},
    {0x340A50, 356},
    {0x34B940, 545},
}};
};
struct GameDriver {
    std::uint32_t graphics_started_rva = 0xB2A9000;
    std::uint32_t catalog_mutex_rva = 0xB95B690;
};
struct Guide {
struct Pin { std::uint32_t rva, size; const char* sha; };
    Pin builder{0x6833A0, 1632, nullptr};
    Pin factory{0x34AFB0, 932, nullptr};
    Pin renderer{0x683D40, 1718, nullptr};
    Pin capture{0x6840E0, 6,
    "2c96a8932afbbb93e85faec5ddbd8df4259e98c6ef97e35f91a890c503c26b48"};
    Pin copy_string{0x369660, 205, nullptr};
    Pin grow_vector{0x4137D0, 549, nullptr};
    std::uint32_t guide_build_caller_rva = 0x683E44;
    std::uint32_t guide_create_call_rva = 0x6840E6;
};
struct LevelPopulation {
    std::uint32_t category_manager_rva = 0xAAB3B08;
    std::uint32_t list_vtable_rva = 0xD53278;
};
struct List {
    std::uint32_t pool_rva = 0xB3DF3E0;
    std::uint32_t sprite_vtable = 0xE6C810;
    std::uint32_t background_return_rva = 0x5F7E62;
    std::uint32_t p1_return_rva = 0x5F865C;
    std::uint32_t p2_return_rva = 0x5F86AB;
    std::uint32_t solo_return_rva = 0x5F876D;
};
struct ListChart {
    std::uint32_t p1_site = 0x5F865C;
    std::uint32_t p2_site = 0x5F86AB;
    std::uint32_t solo_site = 0x5F876D;
    std::uint32_t normal_names = 0x105D0F0;
    std::uint32_t compact_names = 0x105D300;
};
struct ListContract {
struct Binding { std::uint32_t slot, target; };
struct Import { std::uint32_t slot; std::uint16_t ordinal; };
struct Pin { std::uint32_t rva, size; };
    std::array<Pin, 11> game_code{{
    {0x949480, 55},
    {0x949530, 57},
    {0x949230, 42},
    {0x34B360, 856},
    {0x34C990, 186},
    {0x34C240, 19},
    {0x353A90, 82},
    {0x34C0F0, 112},
    {0x34C3B0, 156},
    {0x34C450, 119},
    {0x34C720, 27},
}};
    std::array<Binding, 6> game_tables{{
    {0xE6C838, 0x34C240}, // visible, vtable +28
    {0xE6C860, 0x34C3B0}, // emit's +50 matrix initialization
    {0xE6C878, 0x34C450}, // emit's +68 pivot
    {0xE6C898, 0x34C720}, // wrappers compare the +88 flag method
    {0xE6C8E0, 0x34C990}, // emit's +D0 scale
    {0xE6C900, 0x34CAD0}, // initializer compares +F0 before inlining it
}};
    std::array<Import, 13> game_imports{{
    {0xC91B50,56}, {0xC91B88,106}, {0xC91BC8,116},
    {0xC91C40,35}, {0xC91C48,37}, {0xC91C68,45},
    {0xC91C80,47}, {0xC91C88,51}, {0xC91C90,49},
    {0xC91C98,50}, {0xC91CA0,46}, {0xC91CB0,52}, {0xC91CB8,85},
}};
    std::uint32_t chart_player_count_rva = 0x949480;
    std::uint32_t chart_style_rva = 0x949530;
    std::uint32_t chart_active_player_rva = 0x949230;
    std::uint32_t emit_rva = 0x34B360;
    std::uint32_t visible_rva = 0x34C240;
};
struct ListHookContract {
struct Pin { std::uint32_t rva, size; };
    std::array<Pin, 3> pins{{
    {0x34AC50, 856},
    {0x5F7BF0, 997},
    {0x5F8570, 525},
}};
    std::uint32_t background_return_rva = 0x5F7E62;
    std::uint32_t p1_return_rva = 0x5F865C;
    std::uint32_t p2_return_rva = 0x5F86AB;
    std::uint32_t solo_return_rva = 0x5F876D;
};
struct ListTitleContract {
struct Pin { std::uint32_t rva, size; };
    Pin title{0x5F8AC0, 874};
    Pin background{0x5F7BF0, 997};
    std::uint32_t width_rva = 0xCAC1A8;
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
struct Played {
    std::uint32_t layer_vtable = 0xDC86E0;
};
struct PlayedCallsContract {
struct Import { std::uint32_t slot; std::uint16_t ordinal; std::uint32_t target; };
struct Pin { std::uint32_t rva, size; };
    std::array<Pin, 1> game_code{{
    {0x949530, 57},
}};
    std::array<Import, 2> game_imports{{
    {0xC91B88, 106, 0x1DA50}, {0xC91BC8, 116, 0x22D60},
}};
    std::uint32_t chart_style_rva = 0x949530;
};
struct PlayedChart {
    std::uint32_t active_flags = 0xACD79B0;
    std::uint32_t dp_flag = 0xACD79A4;
    std::uint32_t published_music = 0xACD79D0;
    std::uint32_t published_difficulties = 0xACD79A8;
    std::uint32_t layer_vtable = 0xDC86E0;
    std::uint32_t decide_first = 0x601715;
    std::uint32_t decide_second = 0x601749;
    std::uint32_t hud_site = 0x9045FD;
    std::uint32_t result_site = 0x91C56D;
    std::uint32_t stepup_site = 0x601F4E;
    std::uint32_t stepup_owner_offset = 0x518;
    std::uint32_t stepup_vtable = 0xD818B8;
    std::uint32_t stepup_callback = 0x8D9540;
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
struct ResultNotes {
    std::uint32_t layer_vtable = 0xDC86E0;
};
struct ResultPlate {
    std::uint32_t layer_vtable = 0xDC86E0;
};
struct SongSelect {
    std::uint32_t category_data = 0xAAB3B08;
    std::uint32_t category_list = 0xD53278;
    std::array<std::uintptr_t, 12> groups{
    0xCB6D88, 0xCB6B10, 0xCB70B0, 0xCB6BC0, 0xCB6C70, 0xCB6478,
    0xCB63D0, 0, 0xCB69D0, 0xCB6E60, 0xCB6A90, 0xCB0700};
};
struct SortContract {
struct Pin { std::uint32_t rva, size; };
    std::array<Pin, 18> pins{{
    {0x870D60, 668},
    {0xB776D0, 820},
    {0xB775D0, 204},
    {0x871250, 252},
    {0x871610, 447},
    {0x9522B0, 33},
    {0x9493C0, 7},
    {0x82DFA0, 160},
    {0x5F97A0, 4},
    {0x9520A0, 164},
    {0xB7E4BC, 71},
    {0x949530, 57},
    // Immediate bar payload/type accessors; never infer visible from contents.
    {0x5FEF00, 5},
    {0x5FEF30, 3},
    // Direct qsort validation/cookie helpers and its unwind handler.
    {0xB7A2E0, 32},
    {0xB77414, 30},
    {0xB1BC50, 33},
    {0xB1BB40, 29},
}};
    std::uint32_t qsort_rva = 0xB776D0;
    std::uint32_t dispatcher_call_rva = 0x870F83;
    std::uint32_t comparator_selection_rva = 0x870E37;
    std::uint32_t mode_rva = 0x9493C0;
    std::uint32_t level_rva = 0x871250;
    std::uint32_t sort_return_rva = 0x870F88;
};
struct StepupContract {
struct Pin { std::uint32_t rva, size; };
    std::array<Pin, 5> pins{{
    {0x601E80, 522},
    {0x3430E0, 41},
    {0x8D4EE0, 338},
    {0x8D9540, 816},
    {0x602BB0, 91},
}};
    std::uint32_t capture_rva = 0x601F4E;
    std::uint32_t named_return_rva = 0x601F5B;
};
struct SurfaceContract {
struct Pin { std::uint32_t rva, size; };
    Pin cup{0x6001D0, 1467};
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
    build_bindings::AvsRead avs_read;
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
    build_bindings::FolderContract folder_contract;
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
    build_bindings::Played played;
    build_bindings::PlayedCallsContract played_calls_contract;
    build_bindings::PlayedChart played_chart;
    build_bindings::PlayedContract played_contract;
    build_bindings::ResultNotes result_notes;
    build_bindings::ResultPlate result_plate;
    build_bindings::SongSelect song_select;
    build_bindings::SortContract sort_contract;
    build_bindings::StepupContract stepup_contract;
    build_bindings::SurfaceContract surface_contract;
};
extern const GameBindings august_bindings;
extern const GameBindings september_bindings;
inline const GameBindings& game_bindings(ImageRange image) noexcept { return *image.bindings; }
}
