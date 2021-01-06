#include "launcher_window.h"

#include <stdio.h>
#include <stdbool.h>
#include <memory.h>

#include "libgamestream/errors.h"

#include "backend/application_manager.h"
#include "gui_root.h"
#include "settings_window.h"

#define LINKEDLIST_TYPE PSERVER_LIST
#include "util/linked_list.h"

static PSERVER_LIST selected_server_node;

typedef enum pairing_state
{
    PS_NONE,
    PS_RUNNING,
    PS_FAIL
} pairing_state;

static struct
{
    pairing_state state;
    char pin[5];
    char *error;
} pairing_computer_state;

static struct nk_style_button cm_list_button_style;

static void _select_computer(PSERVER_LIST node, bool load_apps);
static void _open_pair(int index, PSERVER_LIST node);

static void _pairing_window(struct nk_context *ctx);
static void _pairing_error_popup(struct nk_context *ctx);
static void _server_error_popup(struct nk_context *ctx);

static bool cw_computer_dropdown(struct nk_context *ctx, PSERVER_LIST list, bool event_emitted);

void launcher_window_init(struct nk_context *ctx)
{
    selected_server_node = NULL;
    pairing_computer_state.state = PS_NONE;
    memcpy(&cm_list_button_style, &(ctx->style.button), sizeof(struct nk_style_button));
    cm_list_button_style.text_alignment = NK_TEXT_ALIGN_LEFT;
}

bool launcher_window(struct nk_context *ctx)
{
    /* GUI */
    int content_width_remaining, content_height_remaining;
    int window_flags = NK_WINDOW_BORDER | NK_WINDOW_CLOSABLE | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_TITLE;
    if (pairing_computer_state.state == PS_RUNNING || gui_settings_showing)
    {
        window_flags |= NK_WINDOW_NO_INPUT;
    }
    if (nk_begin(ctx, "Moonlight", nk_rect(60, 50, gui_display_width - 120, gui_display_height - 100),
                 window_flags))
    {
        bool event_emitted = false;
        struct nk_vec2 content_size = nk_window_get_content_inner_size(ctx);
        content_height_remaining = (int)content_size.y;

        content_width_remaining = (int)content_size.x;
        nk_menubar_begin(ctx);
        nk_layout_row_template_begin(ctx, 25);
        nk_layout_row_template_push_static(ctx, 150);
        nk_layout_row_template_push_variable(ctx, 10);
        nk_layout_row_template_push_static(ctx, 80);
        nk_layout_row_template_end(ctx);

        content_height_remaining -= nk_widget_bounds(ctx).h;

        int computer_len = linkedlist_len(computer_list);
        event_emitted |= cw_computer_dropdown(ctx, computer_list, event_emitted);

        nk_spacing(ctx, 1);

        if (nk_button_label(ctx, "Settings"))
        {
            settings_window_open();
        }
        nk_menubar_end(ctx);

        struct nk_list_view list_view;
        nk_layout_row_dynamic(ctx, content_height_remaining, 1);
        PSERVER_LIST selected = selected_server_node;

        if (selected != NULL)
        {
            if (selected->server != NULL)
            {
                event_emitted |= cw_application_list(ctx, selected, event_emitted);
            }
            else
            {
                _server_error_popup(ctx);
            }
        }
        else
        {
            nk_label(ctx, "Not selected", NK_TEXT_ALIGN_LEFT);

            if (pairing_computer_state.state == PS_FAIL)
            {
                _pairing_error_popup(ctx);
            }
        }
    }
    nk_end(ctx);

    if (pairing_computer_state.state == PS_RUNNING)
    {
        _pairing_window(ctx);
    }

    // Why Nuklear why, the button looks like "close" but it actually "hide"
    if (nk_window_is_hidden(ctx, "Moonlight"))
    {
        nk_window_close(ctx, "Moonlight");
        return false;
    }
    return true;
}

bool cw_computer_dropdown(struct nk_context *ctx, PSERVER_LIST list, bool event_emitted)
{
    char *selected = selected_server_node != NULL ? selected_server_node->name : "Computer";
    if (nk_combo_begin_label(ctx, selected, nk_vec2(150, 200)))
    {
        nk_layout_row_dynamic(ctx, 25, 1);
        PSERVER_LIST cur = list;
        int i = 0;
        while (cur != NULL)
        {
            if (nk_combo_item_label(ctx, cur->name, NK_TEXT_LEFT))
            {
                if (!event_emitted)
                {
                    SERVER_DATA *server = (SERVER_DATA *)cur->server;
                    if (server == NULL)
                    {
                        _select_computer(cur, false);
                    }
                    else if (server->paired)
                    {
                        _select_computer(cur, cur->apps == NULL);
                    }
                    else
                    {
                        _open_pair(i, cur);
                    }
                    event_emitted = true;
                }
            }
            cur = cur->next;
            i++;
        }
        // nk_combo_item_label(ctx, "Add manually", NK_TEXT_LEFT);
        nk_combo_end(ctx);
    }
    return event_emitted;
}

void _select_computer(PSERVER_LIST node, bool load_apps)
{
    selected_server_node = node;
    pairing_computer_state.state = PS_NONE;
    if (load_apps)
    {
        application_manager_load(node);
    }
}

static void cw_pairing_callback(PSERVER_LIST node, int result, const char *error)
{
    if (result == GS_OK)
    {
        // Close pairing window
        pairing_computer_state.state = PS_NONE;
        _select_computer(node, node->apps == NULL);
    }
    else
    {
        // Show pairing error instead
        pairing_computer_state.state = PS_FAIL;
        pairing_computer_state.error = (char *)error;
    }
}

void _open_pair(int index, PSERVER_LIST node)
{
    selected_server_node = NULL;
    pairing_computer_state.state = PS_RUNNING;
    computer_manager_pair(node, &pairing_computer_state.pin[0], cw_pairing_callback);
}

void _pairing_window(struct nk_context *ctx)
{
    static const struct nk_vec2 size = {330, 110};
    struct nk_vec2 pos = {(gui_display_width - size.x) / 2, (gui_display_height - size.y) / 2};
    struct nk_rect s = nk_recta(pos, size);
    if (nk_begin(ctx, "Pairing", s, NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NOT_INTERACTIVE | NK_WINDOW_NO_SCROLLBAR))
    {
        nk_layout_row_dynamic(ctx, 64, 1);

        nk_labelf_wrap(ctx, "Please enter %s on your GameStream PC. This dialog will close when pairing is completed.",
                       pairing_computer_state.pin);
    }
    nk_end(ctx);
}

void _pairing_error_popup(struct nk_context *ctx)
{
    static struct nk_rect s = {34, 40, 220, 110};
    if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Pairing Failed",
                       NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR, s))
    {
        struct nk_vec2 content_size = nk_window_get_content_inner_size(ctx);
        int content_height_remaining = (int)content_size.y;
        /* remove bottom button height */
        content_height_remaining -= 30;
        nk_layout_row_dynamic(ctx, content_height_remaining, 1);

        if (pairing_computer_state.error != NULL)
        {
            nk_label_wrap(ctx, pairing_computer_state.error);
        }
        else
        {
            nk_label_wrap(ctx, "Pairing error.");
        }
        nk_layout_space_begin(ctx, NK_STATIC, 30, 1);
        nk_layout_space_push(ctx, nk_recti(content_size.x - 80, 0, 80, 30));
        if (nk_button_label(ctx, "OK"))
        {
            pairing_computer_state.state = PS_NONE;
            nk_popup_close(ctx);
        }
        nk_layout_space_end(ctx);
        nk_popup_end(ctx);
    }
}

void _server_error_popup(struct nk_context *ctx)
{
    static struct nk_rect s = {34, 40, 220, 110};
    if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Connection Error",
                       NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR, s))
    {
        struct nk_vec2 content_size = nk_window_get_content_inner_size(ctx);
        int content_height_remaining = (int)content_size.y;
        /* remove bottom button height */
        content_height_remaining -= 30;
        nk_layout_row_dynamic(ctx, 40, 1);
        nk_label_wrap(ctx, selected_server_node->errmsg);
        nk_layout_space_begin(ctx, NK_STATIC, 30, 1);
        nk_layout_space_push(ctx, nk_recti(content_size.x - 80, 0, 80, 30));
        if (nk_button_label(ctx, "OK"))
        {
            selected_server_node = NULL;
            nk_popup_close(ctx);
        }
        nk_popup_end(ctx);
    }
}