#include "EditorExport.h"
#include <Hooking.h>
#include "GameVersion.h"

namespace EditorExport
{
    static constexpr uint32_t kExportBitrate = 100000000;   // 100 Mbit/s

    // VBR quality percent (Win8+ path uses this, not avg-bitrate). 100 = max.
    static constexpr uint32_t kQualityPercent = 100;

    // Float written to the "60 FPS" toggle slot. The editor re-renders frame by
    // frame on export, so this yields a real higher framerate. 0x42F00000 = 120.0f.
    static constexpr float kHighFps = 120.0f;

    // H.264 level. 0x2A=4.2 (stock, caps ~62 Mbit @1080p). 0x33=5.1 lifts that to
    // ~240 Mbit and is the highest level Microsoft's H.264 encoder MFT accepts;
    // 5.2 (0x34) is rejected → "unknown error" on export. eAVEncH264VLevel5_1.
    static constexpr uint32_t kH264Level = 0x33;

    // Max export resolution (stock 1920x1080). The editor clamps its output to
    // this, so even a 4K source gets downscaled. 3840x2160 = native 4K UHD.
    // NOTE: the MS encoder caps at level 5.1, which only covers 4K up to ~30 fps.
    // 4K + the 120fps toggle may exceed it; use the 30fps option for 4K if so.
    static constexpr uint32_t kMaxWidth = 3840;
    static constexpr uint32_t kMaxHeight = 2160;
    // -------------------------------------------------------------------------

    void Initialize()
    {
        if (!gameversion::IsLegacy())
            return;
        {
            auto p = hook::pattern("48 8D 3D ? ? ? ? 41 8B C8 8B DA");
            if (p.size() > 0)
            {
                auto* table = hook::get_address<uint32_t*>(p.get_first<uint8_t>(0), 3, 7);
                for (int i = 0; i < 12; ++i)
                    hook::put<uint32_t>(&table[i], kExportBitrate);
            }
        }

        {
            auto p = hook::pattern("48 8D 0D ? ? ? ? 48 98 8B 04 81 EB ?");
            if (p.size() > 0)
            {
                auto* table = hook::get_address<uint32_t*>(p.get_first<uint8_t>(0), 3, 7);
                for (int i = 0; i < 12; ++i)
                    hook::put<uint32_t>(&table[i], kQualityPercent);
            }
        }

        {
            auto p = hook::pattern("48 8D 0D ? ? ? ? F3 0F 10 04 81 C3 F3 0F 10 05 ? ? ? ?");
            if (p.size() > 0)
            {
                auto* table = hook::get_address<float*>(p.get_first<uint8_t>(0), 3, 7);
                hook::put<float>(&table[1], kHighFps);
            }
        }

        {
            auto p = hook::pattern("41 B8 2A 00 00 00 48 8B 01");
            if (p.size() > 0)
                hook::put<uint32_t>(p.get_first<uint32_t>(2), kH264Level);
        }

        {
            auto pw = hook::pattern("B8 80 07 00 00 45 8B D0");
            if (pw.size() > 0)
                hook::put<uint32_t>(pw.get_first<uint32_t>(1), kMaxWidth);

            auto ph = hook::pattern("B9 38 04 00 00 41 8B C1");
            if (ph.size() > 0)
                hook::put<uint32_t>(ph.get_first<uint32_t>(1), kMaxHeight);
        }
    }
}

static HookFunction hookEditorExport([]()
{
    EditorExport::Initialize();
});
