#include "core/font/msdf_wrapper.h"

#include "core/logger.h"
#include "memory/memory.h"
#include "../vendor/msdf-atlas-gen/msdf-atlas-gen/msdf-atlas-gen.h"

using namespace msdf_atlas;

#define PIXEL_RANGE 4.0f
#define FONT_SCALE 48.0f

b32 msdf_load_font_ascii(const char *path, rl_font *out_font) {
    msdfgen::FreetypeHandle *ft_handle = msdfgen::initializeFreetype();

    if (ft_handle == nullptr) {
        RL_ERROR("rl_msdf_load_font(): failed to initialize FreeType");
        return false;
    }

    auto font = msdfgen::loadFont(ft_handle, path);
    if (!font) {
        RL_ERROR("rl_msdf_load_font(): failed to load font '%s'", path);
        msdfgen::deinitializeFreetype(ft_handle);
        return false;
    }

    std::vector<GlyphGeometry> glyphs;
    FontGeometry fontGeometry(&glyphs);
    fontGeometry.loadCharset(font, 1.0, Charset::ASCII);

    // Apply MSDF edge coloring. See edge-coloring.h for other coloring strategies.
    constexpr double maxCornerAngle = 3.0;
    for (GlyphGeometry &glyph : glyphs)
        glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, maxCornerAngle, 0);

    // TightAtlasPacker class computes the layout of the atlas.
    TightAtlasPacker packer;
    // Set atlas parameters:
    // setDimensions or setDimensionsConstraint to find the best value
    packer.setDimensionsConstraint(DimensionsConstraint::SQUARE);
    // setScale for a fixed size or setMinimumScale to use the largest that fits
    packer.setMinimumScale(FONT_SCALE);
    // setPixelRange or setUnitRange
    packer.setPixelRange(PIXEL_RANGE);
    packer.setMiterLimit(1.0);
    // Compute atlas layout - pack glyphs
    packer.pack(glyphs.data(), glyphs.size());
    // Get final atlas dimensions
    int width = 0, height = 0;
    packer.getDimensions(width, height);
    // The ImmediateAtlasGenerator class facilitates the generation of the atlas bitmap.
    ImmediateAtlasGenerator<
        float, // pixel type of buffer for individual glyphs depends on generator function
        4, // number of atlas color channels
        mtsdfGenerator, // function to generate bitmaps for individual glyphs
        BitmapAtlasStorage<byte, 4> // class that stores the atlas bitmap
        // For example, a custom atlas storage class that stores it in VRAM can be used.
    > generator(width, height);
    // GeneratorAttributes can be modified to change the generator's default settings.
    GeneratorAttributes attributes;
    generator.setAttributes(attributes);
    generator.setThreadCount(4);
    // Generate atlas bitmap
    generator.generate(glyphs.data(), glyphs.size());
    // The atlas bitmap can now be retrieved via atlasStorage as a BitmapConstRef.
    // The glyphs array (or fontGeometry) contains positioning data for typesetting text.

    out_font->glyph_count = static_cast<u32>(glyphs.size());
    out_font->glyphs = static_cast<rl_glyph *>(rl_alloc(
        sizeof(rl_glyph) * out_font->glyph_count,
        MEM_SUBSYSTEM_ASSET
        ));

    out_font->ascender = static_cast<float>(fontGeometry.getMetrics().ascenderY);
    out_font->descender = static_cast<float>(fontGeometry.getMetrics().descenderY);
    out_font->line_height = fontGeometry.getMetrics().lineHeight;
    out_font->pixel_range = PIXEL_RANGE;
    out_font->scale = FONT_SCALE;

    for (u32 i = 0; i < out_font->glyph_count; i++) {
        const GlyphGeometry &g = glyphs[i];
        rl_glyph *dst = &out_font->glyphs[i];

        dst->codepoint = g.getCodepoint();
        dst->advance = static_cast<f32>(g.getAdvance());

        double pl, pb, pr, pt;
        g.getQuadPlaneBounds(pl, pb, pr, pt);
        dst->plane_min_x = static_cast<f32>(pl);
        dst->plane_min_y = static_cast<f32>(pb);
        dst->plane_max_x = static_cast<f32>(pr);
        dst->plane_max_y = static_cast<f32>(pt);

        double al, ab, ar, at;
        g.getQuadAtlasBounds(al, ab, ar, at);
        dst->uv_min_x = static_cast<f32>(al / width);
        dst->uv_min_y = static_cast<f32>(ab / height);
        dst->uv_max_x = static_cast<f32>(ar / width);
        dst->uv_max_y = static_cast<f32>(at / height);
    }

    auto &storage = generator.atlasStorage();

    out_font->atlas.width = width;
    out_font->atlas.height = height;
    out_font->atlas.channels = 4;

    u64 size = static_cast<u64>(width) * height * 4;
    out_font->atlas.size = size;
    out_font->atlas.data = static_cast<u8 *>(rl_alloc(size, MEM_SUBSYSTEM_ASSET));

    // Create a *view* over our buffer
    msdfgen::BitmapRef<byte, 4> bitmap(
        out_font->atlas.data,
        width,
        height
        );

    // Copy atlas into our buffer
    storage.get(0, 0, bitmap);

    msdfgen::destroyFont(font);
    msdfgen::deinitializeFreetype(ft_handle);
    return true;
}
