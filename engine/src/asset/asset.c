#include "asset.h"

const char *get_assets_dir(ASSET_TYPE asset_type) {
    switch (asset_type) {
    case ASSET_FONT:
        return "../../../assets/fonts/";
    }

    return "../../../assets/";
}