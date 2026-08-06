#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "nvbios_reader.hpp"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#ifdef _MSC_VER
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

namespace {

constexpr wchar_t window_class_name[] = L"NvidiaBiosReaderWindow";
constexpr UINT command_open = 1001;
constexpr UINT command_save = 1002;
constexpr UINT control_memory = 1101;
constexpr UINT control_timings = 1102;
constexpr UINT control_status = 1103;

std::wstring widen(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        return L"?";
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
    return result;
}

std::wstring hex_offset(std::size_t value) {
    std::wostringstream out;
    out << L"0x" << std::uppercase << std::hex << value;
    return out.str();
}

std::wstring decimal_range(std::uint16_t low, std::uint16_t high) {
    return std::to_wstring(low) + L" - " + std::to_wstring(high);
}

std::wstring displayed_range(double low, double high) {
    std::wostringstream out;
    out << std::fixed << std::setprecision(2) << low << L" - " << high << L" MHz";
    return out.str();
}

void set_text(HWND window, const std::wstring& text) {
    SetWindowTextW(window, text.c_str());
}

void set_list_text(HWND list, int row, int column, const std::wstring& text) {
    LVITEMW item{};
    item.iSubItem = column;
    item.pszText = const_cast<wchar_t*>(text.c_str());
    ListView_SetItemText(list, row, column, item.pszText);
}

void add_column(HWND list, int column, const wchar_t* title, int width) {
    LVCOLUMNW item{};
    item.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    item.pszText = const_cast<wchar_t*>(title);
    item.cx = width;
    item.iSubItem = column;
    ListView_InsertColumn(list, column, &item);
}

class Application {
public:
    bool create(HINSTANCE instance, int show_command) {
        instance_ = instance;
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.style = CS_HREDRAW | CS_VREDRAW;
        window_class.lpfnWndProc = &Application::window_proc;
        window_class.hInstance = instance;
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        window_class.hIconSm = window_class.hIcon;
        window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        window_class.lpszClassName = window_class_name;
        if (!RegisterClassExW(&window_class)) {
            return false;
        }

        window_ = CreateWindowExW(
            0,
            window_class_name,
            L"NVIDIA BIOS Reader",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1040,
            760,
            nullptr,
            nullptr,
            instance,
            this);
        if (!window_) {
            return false;
        }
        ShowWindow(window_, show_command == SW_HIDE ? SW_SHOWNORMAL : show_command);
        UpdateWindow(window_);
        return true;
    }

    int run() {
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return static_cast<int>(message.wParam);
    }

    void load(const std::filesystem::path& path) {
        try {
            set_status(L"Reading VBIOS...");
            auto next = std::make_unique<nvbr::Document>(nvbr::inspect_vbios(path));
            document_ = std::move(next);
            populate_document();
            set_status(L"VBIOS loaded successfully. The ROM was not modified.");
        } catch (const std::exception& error) {
            const std::wstring message = L"Could not read this VBIOS.\n\n" + widen(error.what());
            MessageBoxW(window_, message.c_str(), L"NVIDIA BIOS Reader", MB_OK | MB_ICONERROR);
            set_status(L"Could not load the selected file.");
        }
    }

private:
    static LRESULT CALLBACK window_proc(
        HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
        Application* app = reinterpret_cast<Application*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            app = static_cast<Application*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
            app->window_ = window;
        }
        return app ? app->handle_message(message, wparam, lparam)
                   : DefWindowProcW(window, message, wparam, lparam);
    }

    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
        switch (message) {
            case WM_CREATE:
                create_controls();
                return 0;
            case WM_CTLCOLORSTATIC: {
                HDC device = reinterpret_cast<HDC>(wparam);
                SetBkMode(device, TRANSPARENT);
                return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
            }
            case WM_SIZE:
                layout_controls(LOWORD(lparam), HIWORD(lparam));
                return 0;
            case WM_COMMAND:
                if (LOWORD(wparam) == command_open) {
                    open_dialog();
                    return 0;
                }
                if (LOWORD(wparam) == command_save) {
                    save_dialog();
                    return 0;
                }
                break;
            case WM_NOTIFY:
                return handle_notify(reinterpret_cast<NMHDR*>(lparam));
            case WM_DROPFILES:
                handle_drop(reinterpret_cast<HDROP>(wparam));
                return 0;
            case WM_GETMINMAXINFO: {
                auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
                info->ptMinTrackSize.x = scale(760);
                info->ptMinTrackSize.y = scale(600);
                return 0;
            }
            case WM_DESTROY:
                if (font_) {
                    DeleteObject(font_);
                    font_ = nullptr;
                }
                if (title_font_) {
                    DeleteObject(title_font_);
                    title_font_ = nullptr;
                }
                if (section_font_) {
                    DeleteObject(section_font_);
                    section_font_ = nullptr;
                }
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProcW(window_, message, wparam, lparam);
    }

    int scale(int value) const {
        const UINT dpi = window_ ? GetDpiForWindow(window_) : 96;
        return MulDiv(value, static_cast<int>(dpi), 96);
    }

    HWND create_control(
        DWORD ex_style,
        const wchar_t* class_name,
        const wchar_t* text,
        DWORD style,
        int id = 0) {
        HWND control = CreateWindowExW(
            ex_style,
            class_name,
            text,
            WS_CHILD | WS_VISIBLE | style,
            0,
            0,
            0,
            0,
            window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            instance_,
            nullptr);
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        return control;
    }

    HWND create_label(const wchar_t* text, DWORD style = SS_LEFT) {
        return create_control(0, L"STATIC", text, style);
    }

    void create_controls() {
        NONCLIENTMETRICSW metrics{};
        metrics.cbSize = sizeof(metrics);
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
        font_ = CreateFontIndirectW(&metrics.lfMessageFont);

        LOGFONTW title_font = metrics.lfMessageFont;
        title_font.lfHeight = -MulDiv(15, static_cast<int>(GetDpiForWindow(window_)), 72);
        title_font.lfWeight = FW_SEMIBOLD;
        title_font_ = CreateFontIndirectW(&title_font);

        LOGFONTW section_font = metrics.lfMessageFont;
        section_font.lfWeight = FW_SEMIBOLD;
        section_font_ = CreateFontIndirectW(&section_font);

        title_ = create_label(L"NVIDIA BIOS Reader", SS_LEFT);
        subtitle_ = create_label(L"", SS_LEFT);
        set_text(subtitle_, L"Read-only VBIOS memory analysis - v" + widen(nvbr::version));
        SendMessageW(title_, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
        open_button_ = create_control(
            0, L"BUTTON", L"Open VBIOS...", BS_PUSHBUTTON | WS_TABSTOP, command_open);
        save_button_ = create_control(
            0, L"BUTTON", L"Save Report...", BS_PUSHBUTTON | WS_TABSTOP, command_save);
        EnableWindow(save_button_, FALSE);

        summary_caption_ = create_label(L"VBIOS Summary");
        SendMessageW(summary_caption_, WM_SETFONT, reinterpret_cast<WPARAM>(section_font_), TRUE);
        chip_caption_ = create_label(L"GPU Chip");
        device_caption_ = create_label(L"Device ID");
        version_caption_ = create_label(L"VBIOS Version");
        file_caption_ = create_label(L"File");
        chip_value_ = create_label(L"-");
        device_value_ = create_label(L"-");
        version_value_ = create_label(L"-");
        file_value_ = create_label(L"Open or drop a VBIOS file");

        memory_caption_ = create_label(L"Memory Support");
        SendMessageW(memory_caption_, WM_SETFONT, reinterpret_cast<WPARAM>(section_font_), TRUE);
        memory_list_ = create_control(
            WS_EX_CLIENTEDGE,
            WC_LISTVIEWW,
            L"",
            LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP,
            control_memory);
        ListView_SetExtendedListViewStyle(
            memory_list_, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        add_column(memory_list_, 0, L"Entry", scale(58));
        add_column(memory_list_, 1, L"Type", scale(82));
        add_column(memory_list_, 2, L"Vendor", scale(92));
        add_column(memory_list_, 3, L"Density", scale(88));
        add_column(memory_list_, 4, L"Organization", scale(190));
        add_column(memory_list_, 5, L"Physical Straps", scale(140));
        add_column(memory_list_, 6, L"Timing Status", scale(120));

        details_caption_ = create_label(L"Selected Memory Profile");
        SendMessageW(details_caption_, WM_SETFONT, reinterpret_cast<WPARAM>(section_font_), TRUE);
        profile_value_ = create_label(L"Select a memory entry to inspect its timing map.");
        descriptor_value_ = create_label(L"Descriptor: -");
        timing_list_ = create_control(
            WS_EX_CLIENTEDGE,
            WC_LISTVIEWW,
            L"",
            LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP,
            control_timings);
        ListView_SetExtendedListViewStyle(
            timing_list_, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        add_column(timing_list_, 0, L"Range", scale(56));
        add_column(timing_list_, 1, L"Raw Clock", scale(110));
        add_column(timing_list_, 2, L"Displayed Clock (inferred)", scale(180));
        add_column(timing_list_, 3, L"Timing ID", scale(78));
        add_column(timing_list_, 4, L"Map Offset", scale(92));
        add_column(timing_list_, 5, L"Record Offset", scale(100));
        add_column(timing_list_, 6, L"State", scale(96));

        decoded_caption_ = create_label(L"Decoded CONFIG0..CONFIG5 fields");
        SendMessageW(decoded_caption_, WM_SETFONT, reinterpret_cast<WPARAM>(section_font_), TRUE);
        decoded_value_ = create_control(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"Select a timing range to view the known fields.",
            ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL);

        status_bar_ = create_control(
            0, STATUSCLASSNAMEW, L"Open or drop an NVIDIA VBIOS file.", SBARS_SIZEGRIP, control_status);
        DragAcceptFiles(window_, TRUE);
    }

    void layout_controls(int width, int height) {
        if (!status_bar_) {
            return;
        }
        SendMessageW(status_bar_, WM_SIZE, 0, 0);
        RECT status_rect{};
        GetWindowRect(status_bar_, &status_rect);
        const int status_height = status_rect.bottom - status_rect.top;
        const int margin = scale(14);
        const int gap = scale(8);
        const int content_width = std::max(0, width - 2 * margin);

        const int button_width = scale(122);
        const int button_height = scale(30);
        MoveWindow(open_button_, width - margin - 2 * button_width - gap, margin,
                   button_width, button_height, FALSE);
        MoveWindow(save_button_, width - margin - button_width, margin,
                   button_width, button_height, FALSE);
        MoveWindow(title_, margin, margin - scale(2), scale(260), scale(27), FALSE);
        MoveWindow(subtitle_, margin, margin + scale(25), scale(340), scale(18), FALSE);

        const int summary_top = margin + scale(48);
        const int summary_height = scale(104);
        MoveWindow(summary_caption_, margin, summary_top, content_width, scale(22), FALSE);

        const int label_width = scale(92);
        const int value_width = scale(170);
        const int row1 = summary_top + scale(28);
        const int row2 = summary_top + scale(61);
        int x = margin + scale(16);
        MoveWindow(chip_caption_, x, row1, label_width, scale(20), FALSE);
        MoveWindow(chip_value_, x + label_width, row1, value_width, scale(20), FALSE);
        x += label_width + value_width + scale(20);
        MoveWindow(device_caption_, x, row1, label_width, scale(20), FALSE);
        MoveWindow(device_value_, x + label_width, row1, value_width, scale(20), FALSE);
        x += label_width + value_width + scale(10);
        MoveWindow(version_caption_, x, row1, label_width, scale(20), FALSE);
        MoveWindow(version_value_, x + label_width, row1,
                   std::max(scale(120), width - margin - (x + label_width + scale(12))),
                   scale(20), FALSE);
        x = margin + scale(16);
        MoveWindow(file_caption_, x, row2, label_width, scale(20), FALSE);
        MoveWindow(file_value_, x + label_width, row2,
                   content_width - label_width - scale(32), scale(20), FALSE);

        const int memory_title_top = summary_top + summary_height + scale(10);
        MoveWindow(memory_caption_, margin, memory_title_top,
                   content_width, scale(22), FALSE);
        const int memory_top = memory_title_top + scale(23);
        const int available = height - status_height - memory_top - margin;
        const int memory_height = std::max(scale(125), available * 42 / 100);
        MoveWindow(memory_list_, margin, memory_top, content_width, memory_height, FALSE);

        const int details_top = memory_top + memory_height + gap;
        const int details_height = std::max(0, height - status_height - details_top - margin);
        MoveWindow(details_caption_, margin, details_top, content_width, scale(22), FALSE);
        MoveWindow(profile_value_, margin + scale(14), details_top + scale(24),
                   content_width - scale(28), scale(20), FALSE);
        MoveWindow(descriptor_value_, margin + scale(14), details_top + scale(45),
                   content_width - scale(28), scale(20), FALSE);
        const int timing_top = details_top + scale(68);
        const int decoded_height = scale(66);
        const int timing_height = std::max(scale(70), details_height - scale(68) - decoded_height - scale(37));
        MoveWindow(timing_list_, margin + scale(12), timing_top,
                   content_width - scale(24), timing_height, FALSE);
        const int decoded_top = timing_top + timing_height + scale(6);
        MoveWindow(decoded_caption_, margin + scale(12), decoded_top,
                   content_width - scale(24), scale(19), FALSE);
        MoveWindow(decoded_value_, margin + scale(12), decoded_top + scale(20),
                   content_width - scale(24), decoded_height, FALSE);

        resize_columns();

        // Moving every child with immediate repaint lets transparent labels
        // and list controls paint over one another during live resizing.
        // Paint the completed layout as one clean frame instead.
        RedrawWindow(
            window_, nullptr, nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }

    void resize_columns() {
        RECT rect{};
        GetClientRect(memory_list_, &rect);
        const int width = std::max(
            1, static_cast<int>(rect.right - rect.left) -
               GetSystemMetrics(SM_CXVSCROLL) - scale(4));
        const std::array<int, 7> memory_weights{7, 10, 11, 11, 23, 18, 20};
        for (int i = 0; i < 7; ++i) {
            const auto index = static_cast<std::size_t>(i);
            const int column_width = width * memory_weights[index] / 100;
            if (memory_column_widths_[index] != column_width) {
                ListView_SetColumnWidth(memory_list_, i, column_width);
                memory_column_widths_[index] = column_width;
            }
        }

        GetClientRect(timing_list_, &rect);
        const int timing_width = std::max(
            1, static_cast<int>(rect.right - rect.left) -
               GetSystemMetrics(SM_CXVSCROLL) - scale(4));
        const std::array<int, 7> timing_weights{7, 13, 23, 10, 13, 14, 20};
        for (int i = 0; i < 7; ++i) {
            const auto index = static_cast<std::size_t>(i);
            const int column_width = timing_width * timing_weights[index] / 100;
            if (timing_column_widths_[index] != column_width) {
                ListView_SetColumnWidth(timing_list_, i, column_width);
                timing_column_widths_[index] = column_width;
            }
        }
    }

    void populate_document() {
        if (!document_) {
            return;
        }
        set_text(chip_value_, widen(document_->chip));
        set_text(device_value_, widen(document_->device_id + " / " + document_->vendor_id));
        set_text(version_value_, widen(document_->vbios_version));
        set_text(file_value_, document_->path.filename().wstring() +
            L"  (" + std::to_wstring(document_->file_size) + L" bytes)");
        set_text(window_, L"NVIDIA BIOS Reader - " + document_->path.filename().wstring());
        EnableWindow(save_button_, TRUE);

        ListView_DeleteAllItems(memory_list_);
        visible_memory_.clear();
        for (std::size_t index = 0; index < document_->memory.size(); ++index) {
            const auto& memory = document_->memory[index];
            if (memory.skipped) {
                continue;
            }
            visible_memory_.push_back(index);
            const int row = ListView_GetItemCount(memory_list_);
            const std::wstring entry = std::to_wstring(memory.entry_number);
            LVITEMW item{};
            item.mask = LVIF_TEXT | LVIF_PARAM;
            item.iItem = row;
            item.pszText = const_cast<wchar_t*>(entry.c_str());
            item.lParam = static_cast<LPARAM>(index);
            ListView_InsertItem(memory_list_, &item);
            set_list_text(memory_list_, row, 1, widen(memory.type));
            set_list_text(memory_list_, row, 2, widen(memory.vendor));
            set_list_text(memory_list_, row, 3, widen(memory.density));
            set_list_text(memory_list_, row, 4, widen(memory.organization));
            set_list_text(memory_list_, row, 5, widen(memory.physical_straps));
            set_list_text(memory_list_, row, 6, widen(memory.coverage));
        }

        if (ListView_GetItemCount(memory_list_) > 0) {
            ListView_SetItemState(
                memory_list_, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(memory_list_, 0, FALSE);
        } else {
            clear_profile();
        }
    }

    std::optional<std::size_t> selected_memory_index() const {
        const int row = ListView_GetNextItem(memory_list_, -1, LVNI_SELECTED);
        if (row < 0) {
            return std::nullopt;
        }
        LVITEMW item{};
        item.mask = LVIF_PARAM;
        item.iItem = row;
        if (!ListView_GetItem(memory_list_, &item)) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(item.lParam);
    }

    void populate_profile() {
        if (!document_) {
            clear_profile();
            return;
        }
        const auto selected = selected_memory_index();
        if (!selected || *selected >= document_->memory.size()) {
            clear_profile();
            return;
        }
        selected_memory_ = *selected;
        const auto& memory = document_->memory[*selected];
        set_text(profile_value_,
            L"Entry " + std::to_wstring(memory.entry_number) +
            L" / strap group " + std::to_wstring(memory.strap_group) +
            L"    " + widen(memory.vendor + " " + memory.type + " " +
                              memory.density + " " + memory.organization) +
            L"    Status: " + widen(memory.coverage));
        std::wostringstream descriptor;
        descriptor << L"Descriptor: 0x" << std::uppercase << std::hex
                   << std::setw(8) << std::setfill(L'0') << memory.descriptor
                   << L"    ROM offset: " << hex_offset(memory.descriptor_offset)
                   << L"    Physical straps: " << widen(memory.physical_straps);
        set_text(descriptor_value_, descriptor.str());

        ListView_DeleteAllItems(timing_list_);
        for (std::size_t index = 0; index < memory.timings.size(); ++index) {
            const auto& timing = memory.timings[index];
            const int row = ListView_GetItemCount(timing_list_);
            const std::wstring range_number = std::to_wstring(timing.range_index);
            LVITEMW item{};
            item.mask = LVIF_TEXT | LVIF_PARAM;
            item.iItem = row;
            item.pszText = const_cast<wchar_t*>(range_number.c_str());
            item.lParam = static_cast<LPARAM>(index);
            ListView_InsertItem(timing_list_, &item);
            set_list_text(timing_list_, row, 1, decimal_range(timing.raw_low, timing.raw_high));
            set_list_text(timing_list_, row, 2,
                timing.unused ? L"Unused map slot" : displayed_range(
                    timing.displayed_low_mhz, timing.displayed_high_mhz));
            set_list_text(timing_list_, row, 3,
                timing.timing_id ? std::to_wstring(*timing.timing_id) : L"FF");
            set_list_text(timing_list_, row, 4, hex_offset(timing.map_byte_offset));
            set_list_text(timing_list_, row, 5,
                timing.timing_record_offset ? hex_offset(*timing.timing_record_offset) : L"-");
            std::wstring state = timing.unused ? L"Unused" :
                (!timing.timing_id ? L"No record" :
                 (timing.all_zero_record ? L"All zero" :
                  (timing.timing_record_offset ? L"Present" : L"Invalid reference")));
            set_list_text(timing_list_, row, 6, state);
        }
        if (ListView_GetItemCount(timing_list_) > 0) {
            ListView_SetItemState(
                timing_list_, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        } else {
            set_text(decoded_value_, L"No timing map is available for this entry.");
        }
    }

    void populate_timing_details() {
        if (!document_ || !selected_memory_) {
            return;
        }
        const int row = ListView_GetNextItem(timing_list_, -1, LVNI_SELECTED);
        if (row < 0) {
            set_text(decoded_value_, L"Select a timing range to view the known fields.");
            return;
        }
        LVITEMW item{};
        item.mask = LVIF_PARAM;
        item.iItem = row;
        if (!ListView_GetItem(timing_list_, &item)) {
            return;
        }
        const auto& memory = document_->memory[*selected_memory_];
        const std::size_t timing_index = static_cast<std::size_t>(item.lParam);
        if (timing_index >= memory.timings.size()) {
            return;
        }
        const auto& timing = memory.timings[timing_index];
        if (timing.unused) {
            set_text(decoded_value_, L"This is an unused 0-0 map slot and is excluded from coverage.");
        } else if (!timing.timing_id) {
            set_text(decoded_value_, L"FF: this memory profile does not reference a timing record for this range.");
        } else if (!timing.timing_record_offset) {
            set_text(decoded_value_, L"The timing ID is outside the timing table and is considered invalid.");
        } else if (timing.all_zero_record) {
            set_text(decoded_value_, L"The referenced timing record contains only zero bytes.");
        } else {
            set_text(decoded_value_, widen(timing.decoded_fields));
        }
    }

    void clear_profile() {
        selected_memory_.reset();
        set_text(profile_value_, L"Select a memory entry to inspect its timing map.");
        set_text(descriptor_value_, L"Descriptor: -");
        ListView_DeleteAllItems(timing_list_);
        set_text(decoded_value_, L"Select a timing range to view the known fields.");
    }

    LRESULT handle_notify(const NMHDR* header) {
        if (!header) {
            return 0;
        }
        if (header->hwndFrom == memory_list_ && header->code == LVN_ITEMCHANGED) {
            const auto* change = reinterpret_cast<const NMLISTVIEW*>(header);
            if ((change->uNewState & LVIS_SELECTED) != 0 &&
                (change->uOldState & LVIS_SELECTED) == 0) {
                populate_profile();
            }
            return 0;
        }
        if (header->hwndFrom == timing_list_ && header->code == LVN_ITEMCHANGED) {
            const auto* change = reinterpret_cast<const NMLISTVIEW*>(header);
            if ((change->uNewState & LVIS_SELECTED) != 0 &&
                (change->uOldState & LVIS_SELECTED) == 0) {
                populate_timing_details();
            }
            return 0;
        }
        if (header->code == NM_CUSTOMDRAW &&
            (header->hwndFrom == memory_list_ || header->hwndFrom == timing_list_)) {
            return custom_draw(reinterpret_cast<const NMLVCUSTOMDRAW*>(header));
        }
        return 0;
    }

    LRESULT custom_draw(const NMLVCUSTOMDRAW* draw) const {
        if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) {
            return CDRF_NOTIFYITEMDRAW;
        }
        if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
            return CDRF_NOTIFYSUBITEMDRAW;
        }
        if (draw->nmcd.dwDrawStage != (CDDS_ITEMPREPAINT | CDDS_SUBITEM)) {
            return CDRF_DODEFAULT;
        }
        if (draw->nmcd.hdr.hwndFrom == memory_list_ && draw->iSubItem == 6 && document_) {
            LVITEMW item{};
            item.mask = LVIF_PARAM;
            item.iItem = static_cast<int>(draw->nmcd.dwItemSpec);
            if (ListView_GetItem(memory_list_, &item)) {
                const auto index = static_cast<std::size_t>(item.lParam);
                if (index < document_->memory.size()) {
                    const auto& status = document_->memory[index].coverage;
                    auto* mutable_draw = const_cast<NMLVCUSTOMDRAW*>(draw);
                    if (status == "FULL") mutable_draw->clrText = RGB(0, 112, 40);
                    else if (status == "PARTIAL") mutable_draw->clrText = RGB(184, 93, 0);
                    else mutable_draw->clrText = RGB(176, 32, 32);
                }
            }
        }
        return CDRF_DODEFAULT;
    }

    void open_dialog() {
        std::array<wchar_t, 32768> path{};
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = window_;
        dialog.lpstrFilter = L"VBIOS ROM files (*.rom;*.bin)\0*.rom;*.bin\0All files (*.*)\0*.*\0";
        dialog.lpstrFile = path.data();
        dialog.nMaxFile = static_cast<DWORD>(path.size());
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
        dialog.lpstrTitle = L"Open NVIDIA VBIOS";
        if (GetOpenFileNameW(&dialog)) {
            load(path.data());
        }
    }

    void save_dialog() {
        if (!document_) {
            return;
        }
        std::array<wchar_t, 32768> path{};
        const std::wstring suggested = document_->path.stem().wstring() + L"-report.txt";
        std::copy(suggested.begin(), suggested.end(), path.begin());
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = window_;
        dialog.lpstrFilter = L"Text report (*.txt)\0*.txt\0All files (*.*)\0*.*\0";
        dialog.lpstrFile = path.data();
        dialog.nMaxFile = static_cast<DWORD>(path.size());
        dialog.lpstrDefExt = L"txt";
        dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        dialog.lpstrTitle = L"Save VBIOS Report";
        if (!GetSaveFileNameW(&dialog)) {
            return;
        }
        std::ofstream output(std::filesystem::path(path.data()), std::ios::binary);
        if (!output) {
            MessageBoxW(window_, L"Could not create the report file.",
                        L"NVIDIA BIOS Reader", MB_OK | MB_ICONERROR);
            return;
        }
        output << document_->detailed_report;
        set_status(L"Detailed report saved successfully.");
    }

    void handle_drop(HDROP drop) {
        std::array<wchar_t, 32768> path{};
        if (DragQueryFileW(drop, 0, path.data(), static_cast<UINT>(path.size())) > 0) {
            load(path.data());
        }
        DragFinish(drop);
    }

    void set_status(const std::wstring& text) {
        if (status_bar_) {
            SendMessageW(status_bar_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text.c_str()));
        }
    }

    HINSTANCE instance_{};
    HWND window_{};
    HFONT font_{};
    HFONT title_font_{};
    HFONT section_font_{};
    HWND title_{};
    HWND subtitle_{};
    HWND open_button_{};
    HWND save_button_{};
    HWND summary_caption_{};
    HWND chip_caption_{};
    HWND device_caption_{};
    HWND version_caption_{};
    HWND file_caption_{};
    HWND chip_value_{};
    HWND device_value_{};
    HWND version_value_{};
    HWND file_value_{};
    HWND memory_caption_{};
    HWND memory_list_{};
    HWND details_caption_{};
    HWND profile_value_{};
    HWND descriptor_value_{};
    HWND timing_list_{};
    HWND decoded_caption_{};
    HWND decoded_value_{};
    HWND status_bar_{};
    std::unique_ptr<nvbr::Document> document_;
    std::vector<std::size_t> visible_memory_;
    std::optional<std::size_t> selected_memory_;
    std::array<int, 7> memory_column_widths_{};
    std::array<int, 7> timing_column_widths_{};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    INITCOMMONCONTROLSEX common_controls{};
    common_controls.dwSize = sizeof(common_controls);
    common_controls.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&common_controls);

    Application application;
    if (!application.create(instance, show_command)) {
        MessageBoxW(nullptr, L"Could not create the application window.",
                    L"NVIDIA BIOS Reader", MB_OK | MB_ICONERROR);
        return 1;
    }

    int argument_count = 0;
    wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
    if (arguments && argument_count > 1) {
        application.load(arguments[1]);
    }
    if (arguments) {
        LocalFree(arguments);
    }
    return application.run();
}
