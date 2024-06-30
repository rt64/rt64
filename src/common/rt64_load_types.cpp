//
// RT64
//

#include "rt64_load_types.h"

namespace RT64 {
    // LoadTile

    extern void to_json(json &j, const LoadTile &loadTile) {
        j["fmt"] = loadTile.fmt;
        j["siz"] = loadTile.siz;
        j["line"] = loadTile.line;
        j["tmem"] = loadTile.tmem;
        j["palette"] = loadTile.palette;
        j["cms"] = loadTile.cms;
        j["cmt"] = loadTile.cmt;
        j["masks"] = loadTile.masks;
        j["maskt"] = loadTile.maskt;
        j["shifts"] = loadTile.shifts;
        j["shiftt"] = loadTile.shiftt;
        j["uls"] = loadTile.uls;
        j["ult"] = loadTile.ult;
        j["lrs"] = loadTile.lrs;
        j["lrt"] = loadTile.lrt;
    }

    extern void from_json(const json &j, LoadTile &loadTile) {
        loadTile.fmt = j.value("fmt", 0);
        loadTile.siz = j.value("siz", 0);
        loadTile.line = j.value("line", 0);
        loadTile.tmem = j.value("tmem", 0);
        loadTile.palette = j.value("palette", 0);
        loadTile.cms = j.value("cms", 0);
        loadTile.cmt = j.value("cmt", 0);
        loadTile.masks = j.value("masks", 0);
        loadTile.maskt = j.value("maskt", 0);
        loadTile.shifts = j.value("shifts", 0);
        loadTile.shiftt = j.value("shiftt", 0);
        loadTile.uls = j.value("uls", 0);
        loadTile.ult = j.value("ult", 0);
        loadTile.lrs = j.value("lrs", 0);
        loadTile.lrt = j.value("lrt", 0);
    }

    // LoadTexture

    extern void to_json(json &j, const LoadTexture &loadTexture) {
        j["address"] = loadTexture.address;
        j["fmt"] = loadTexture.fmt;
        j["siz"] = loadTexture.siz;
        j["width"] = loadTexture.width;
    }

    extern void from_json(const json &j, LoadTexture &loadTexture) {
        loadTexture.address = j.value("address", 0);
        loadTexture.fmt = j.value("fmt", 0);
        loadTexture.siz = j.value("siz", 0);
        loadTexture.width = j.value("width", 0);
    }
};