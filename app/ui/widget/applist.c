#include "ui/launcher_window.h"
#include "backend/application_manager.h"
#include "backend/coverloader.h"
#include "stream/session.h"

#define LINKEDLIST_TYPE PAPP_LIST
#include "util/linked_list.h"

bool cw_application_list(struct nk_context *ctx, PSERVER_LIST node, bool event_emitted)
{
    static struct nk_list_view list_view;
    int app_len = linkedlist_len(node->apps);
    int listwidth = nk_widget_width(ctx);
    int itemwidth = 60 * NK_UI_SCALE, itemheight = 150 * NK_UI_SCALE;
    int colcount = 5, rowcount = app_len / colcount;
    if (app_len && app_len % colcount)
    {
        rowcount++;
    }
    if (nk_list_view_begin(ctx, &list_view, "apps_list", NK_WINDOW_BORDER, itemheight, rowcount))
    {
        nk_layout_row_dynamic(ctx, itemheight, colcount);
        int startidx = list_view.begin * colcount;
        PAPP_LIST cur = linkedlist_nth(node->apps, startidx);
        for (int row = 0; row < list_view.count; row++)
        {
            int col;
            for (col = 0; col < colcount && cur != NULL; col++, cur = cur->next)
            {
                struct nk_image *cover = coverloader_get(cur->id);
                if (cover)
                {
                    nk_image(ctx, *cover);
                }
                else
                {
                    if (nk_list_item_label(ctx, cur->name, NK_TEXT_ALIGN_CENTERED | NK_TEXT_ALIGN_BOTTOM))
                    {
                        if (!event_emitted)
                        {
                            event_emitted |= true;
                            streaming_begin(node->server, cur->id);
                        }
                    }
                }
            }
            if (col < colcount)
            {
                nk_spacing(ctx, colcount - col);
            }
        }

        nk_list_view_end(&list_view);
    }
    return event_emitted;
}