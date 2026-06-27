#include "EditorEncoder.h"

#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <string>
#include <vector>

#include <MinHook.h>
#include <Hooking.h>
#include "GameVersion.h"
#include "Logging.h"
#include "Config.h"

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

namespace EditorEncoder
{
    // Loaded from BetterVideoExport.ini [Encoder] in Initialize().
    static std::wstring kVideoEncodeArgs = L"-c:v libx264 -preset medium -crf 16 -pix_fmt yuv420p";
    static std::wstring kAudioEncodeArgs = L"-c:a aac -b:a 320k";

    static std::wstring g_ffmpegPath;   // empty => feature off, pass through to stock encoder

    static std::string Narrow(const std::wstring& w)
    {
        if (w.empty()) return {};
        int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        std::string s(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
        return s;
    }

    static bool EndsWithI(const std::wstring& s, const wchar_t* suffix)
    {
        size_t n = wcslen(suffix);
        if (s.size() < n) return false;
        return _wcsnicmp(s.c_str() + s.size() - n, suffix, n) == 0;
    }

    static bool WriteAll(HANDLE h, const BYTE* p, DWORD n)
    {
        while (n)
        {
            DWORD wrote = 0;
            if (!WriteFile(h, p, n, &wrote, nullptr) || wrote == 0)
                return false;
            p += wrote;
            n -= wrote;
        }
        return true;
    }

    static HANDLE SpawnFfmpeg(std::wstring cmdLine, HANDLE hStdIn, HANDLE logFile)
    {
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = hStdIn;
        si.hStdOutput = logFile;
        si.hStdError = logFile;

        PROCESS_INFORMATION pi{};
        std::vector<wchar_t> buf(cmdLine.begin(), cmdLine.end());
        buf.push_back(L'\0');

        if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        {
            logging::ErrorF("[EditorEncoder] CreateProcess(ffmpeg) failed: {}", GetLastError());
            return nullptr;
        }
        CloseHandle(pi.hThread);
        return pi.hProcess;
    }

    class ShimSinkWriter : public IMFSinkWriter
    {
    public:
        explicit ShimSinkWriter(LPCWSTR url) : m_finalUrl(url) {}

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
        {
            if (!ppv) return E_POINTER;
            if (riid == __uuidof(IUnknown) || riid == __uuidof(IMFSinkWriter))
            {
                *ppv = static_cast<IMFSinkWriter*>(this);
                AddRef();
                return S_OK;
            }
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
        ULONG STDMETHODCALLTYPE Release() override
        {
            ULONG r = InterlockedDecrement(&m_ref);
            if (r == 0) delete this;
            return r;
        }

        HRESULT STDMETHODCALLTYPE AddStream(IMFMediaType* target, DWORD* pIndex) override
        {
            GUID major = GUID_NULL;
            if (target) target->GetGUID(MF_MT_MAJOR_TYPE, &major);
            DWORD idx = m_nextIndex++;
            if (major == MFMediaType_Audio) m_audioStream = (int)idx;
            else                            m_videoStream = (int)idx; // default to video
            if (pIndex) *pIndex = idx;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE SetInputMediaType(DWORD idx, IMFMediaType* input, IMFAttributes*) override
        {
            if (!input) return S_OK;
            if ((int)idx == m_videoStream)
            {
                MFGetAttributeSize(input, MF_MT_FRAME_SIZE, &m_w, &m_h);
                MFGetAttributeRatio(input, MF_MT_FRAME_RATE, &m_fpsNum, &m_fpsDen);
            }
            else if ((int)idx == m_audioStream)
            {
                input->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &m_sampleRate);
                input->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &m_channels);
                input->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &m_bits);
            }
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE BeginWriting() override
        {
            if (m_w == 0 || m_h == 0 || m_fpsNum == 0)
            {
                logging::ErrorF("[EditorEncoder] missing video format (w={} h={} fps={}/{})", m_w, m_h, m_fpsNum, m_fpsDen);
                return E_FAIL;
            }

            const bool haveAudio = (m_audioStream >= 0);
            m_videoOut = haveAudio ? (m_finalUrl + L".tmpvideo.mp4") : m_finalUrl;

            SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
            m_log = CreateFileW((m_finalUrl + L".ffmpeg.log").c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

            HANDLE rd = nullptr;
            if (!CreatePipe(&rd, &m_videoPipe, &sa, 0))
            {
                logging::ErrorF("[EditorEncoder] CreatePipe failed: {}", GetLastError());
                return E_FAIL;
            }
            SetHandleInformation(m_videoPipe, HANDLE_FLAG_INHERIT, 0);

            std::wstring cmd = L"\"" + g_ffmpegPath + L"\" -y -hide_banner -loglevel error"
                L" -f rawvideo -pixel_format nv12"
                L" -video_size " + std::to_wstring(m_w) + L"x" + std::to_wstring(m_h) +
                L" -framerate " + std::to_wstring(m_fpsNum) + L"/" + std::to_wstring(m_fpsDen) +
                L" -i pipe:0 " + kVideoEncodeArgs + L" \"" + m_videoOut + L"\"";

            m_ffmpeg = SpawnFfmpeg(cmd, rd, m_log);
            CloseHandle(rd); // child owns its copy
            if (!m_ffmpeg)
            {
                CloseHandle(m_videoPipe); m_videoPipe = nullptr;
                return E_FAIL;
            }
            logging::InfoF("[EditorEncoder] encoding {}x{} @ {}/{} fps -> {}", m_w, m_h, m_fpsNum, m_fpsDen, Narrow(m_finalUrl));

            if (haveAudio)
                OpenWav();
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE WriteSample(DWORD idx, IMFSample* sample) override
        {
            if (m_failed) return E_FAIL;
            if (!sample) return S_OK;

            IMFMediaBuffer* buf = nullptr;
            if (FAILED(sample->ConvertToContiguousBuffer(&buf)) || !buf)
                return S_OK;

            BYTE* data = nullptr;
            DWORD maxLen = 0, curLen = 0;
            HRESULT hr = buf->Lock(&data, &maxLen, &curLen);
            if (SUCCEEDED(hr))
            {
                if ((int)idx == m_videoStream && m_videoPipe)
                {
                    if (!WriteAll(m_videoPipe, data, curLen))
                    {
                        logging::ErrorF("[EditorEncoder] video pipe write failed (ffmpeg died?)");
                        m_failed = true;
                    }
                }
                else if ((int)idx == m_audioStream && m_wav)
                {
                    DWORD wrote = 0;
                    WriteFile(m_wav, data, curLen, &wrote, nullptr);
                    m_wavBytes += curLen;
                }
                buf->Unlock();
            }
            buf->Release();
            return m_failed ? E_FAIL : S_OK;
        }

        HRESULT STDMETHODCALLTYPE Finalize() override
        {
            ClosePipeAndWait();      // flush video, let ffmpeg finish encoding
            if (m_wav) { CloseWav(); MuxAudio(); }
            Cleanup();
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Flush(DWORD) override
        {
            if (m_ffmpeg) { TerminateProcess(m_ffmpeg, 1); }
            ClosePipeAndWait();
            if (m_wav) CloseWav();
            DeleteFileW(m_videoOut.c_str());
            DeleteFileW(m_wavPath.c_str());
            Cleanup();
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE SendStreamTick(DWORD, LONGLONG) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE PlaceMarker(DWORD, LPVOID) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE NotifyEndOfSegment(DWORD) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE GetServiceForStream(DWORD, REFGUID, REFIID, LPVOID* ppv) override
        {
            if (ppv) *ppv = nullptr;
            return MF_E_UNSUPPORTED_SERVICE; // GTA only uses this for optional metadata
        }
        HRESULT STDMETHODCALLTYPE GetStatistics(DWORD, MF_SINK_WRITER_STATISTICS*) override { return E_NOTIMPL; }

    private:
        void OpenWav()
        {
            m_wavPath = m_finalUrl + L".tmpaudio.wav";
            m_wav = CreateFileW(m_wavPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (m_wav == INVALID_HANDLE_VALUE) { m_wav = nullptr; return; }
            BYTE hdr[44] = {};
            DWORD w = 0;
            WriteFile(m_wav, hdr, sizeof(hdr), &w, nullptr);
        }

        void CloseWav()
        {
            if (!m_wav) return;
            const uint32_t blockAlign = m_channels * (m_bits / 8);
            const uint32_t byteRate = m_sampleRate * blockAlign;
            const uint32_t dataSize = m_wavBytes;
            const uint32_t riffSize = 36 + dataSize;

            BYTE h[44];
            memcpy(h + 0, "RIFF", 4);              memcpy(h + 4, &riffSize, 4);
            memcpy(h + 8, "WAVE", 4);              memcpy(h + 12, "fmt ", 4);
            uint32_t fmtSize = 16; memcpy(h + 16, &fmtSize, 4);
            uint16_t pcm = 1;      memcpy(h + 20, &pcm, 2);
            uint16_t ch = (uint16_t)m_channels; memcpy(h + 22, &ch, 2);
            memcpy(h + 24, &m_sampleRate, 4);      memcpy(h + 28, &byteRate, 4);
            uint16_t ba = (uint16_t)blockAlign; memcpy(h + 32, &ba, 2);
            uint16_t bits = (uint16_t)m_bits;   memcpy(h + 34, &bits, 2);
            memcpy(h + 36, "data", 4);             memcpy(h + 40, &dataSize, 4);

            SetFilePointer(m_wav, 0, nullptr, FILE_BEGIN);
            DWORD w = 0; WriteFile(m_wav, h, sizeof(h), &w, nullptr);
            CloseHandle(m_wav); m_wav = nullptr;
        }

        void MuxAudio()
        {
            std::wstring cmd = L"\"" + g_ffmpegPath + L"\" -y -hide_banner -loglevel error"
                L" -i \"" + m_videoOut + L"\" -i \"" + m_wavPath + L"\""
                L" -c:v copy " + kAudioEncodeArgs + L" -shortest \"" + m_finalUrl + L"\"";
            HANDLE p = SpawnFfmpeg(cmd, nullptr, m_log);
            if (p)
            {
                WaitForSingleObject(p, INFINITE);
                CloseHandle(p);
                DeleteFileW(m_videoOut.c_str());
                DeleteFileW(m_wavPath.c_str());
            }
        }

        void ClosePipeAndWait()
        {
            if (m_videoPipe) { CloseHandle(m_videoPipe); m_videoPipe = nullptr; }
            if (m_ffmpeg)
            {
                WaitForSingleObject(m_ffmpeg, INFINITE);
                CloseHandle(m_ffmpeg); m_ffmpeg = nullptr;
            }
        }

        void Cleanup()
        {
            if (m_log && m_log != INVALID_HANDLE_VALUE) { CloseHandle(m_log); m_log = nullptr; }
        }

        LONG m_ref = 1;
        std::wstring m_finalUrl, m_videoOut, m_wavPath;
        int m_videoStream = -1, m_audioStream = -1;
        DWORD m_nextIndex = 0;
        UINT32 m_w = 0, m_h = 0, m_fpsNum = 0, m_fpsDen = 1;
        UINT32 m_sampleRate = 48000, m_channels = 2, m_bits = 16;
        UINT32 m_wavBytes = 0;
        HANDLE m_videoPipe = nullptr, m_ffmpeg = nullptr, m_wav = nullptr, m_log = nullptr;
        bool m_failed = false;
    };

    using MFCreateSinkWriterFromURL_t =
        HRESULT(STDAPICALLTYPE*)(LPCWSTR, IMFByteStream*, IMFAttributes*, IMFSinkWriter**);
    static MFCreateSinkWriterFromURL_t g_origCreate = nullptr;

    static HRESULT STDAPICALLTYPE Hooked_Create(LPCWSTR url, IMFByteStream* bs, IMFAttributes* attr, IMFSinkWriter** out)
    {
        if (out && url && !g_ffmpegPath.empty() && EndsWithI(url, L".mp4"))
        {
            *out = new ShimSinkWriter(url);
            return S_OK;
        }
        return g_origCreate(url, bs, attr, out);
    }

    static std::wstring FindFfmpeg()
    {
        wchar_t exe[MAX_PATH];
        if (GetModuleFileNameW(nullptr, exe, MAX_PATH))
        {
            std::wstring dir(exe);
            size_t slash = dir.find_last_of(L"\\/");
            if (slash != std::wstring::npos)
            {
                std::wstring cand = dir.substr(0, slash + 1) + L"ffmpeg.exe";
                if (GetFileAttributesW(cand.c_str()) != INVALID_FILE_ATTRIBUTES)
                    return cand;
            }
        }

        wchar_t found[MAX_PATH];
        if (SearchPathW(nullptr, L"ffmpeg.exe", nullptr, MAX_PATH, found, nullptr))
            return found;
        return {};
    }

    void Initialize()
    {
        if (!gameversion::IsLegacy())
            return;

        Config::EnsureDefaultFile();
        kVideoEncodeArgs = Config::GetStr(L"Encoder", L"VideoArgs", kVideoEncodeArgs.c_str());
        kAudioEncodeArgs = Config::GetStr(L"Encoder", L"AudioArgs", kAudioEncodeArgs.c_str());

        g_ffmpegPath = FindFfmpeg();
        if (g_ffmpegPath.empty())
        {
            logging::Warning("[EditorEncoder] ffmpeg.exe not found (PATH or GTA folder) - "
                             "4K/high-fps editor export disabled; stock encoder will be used.");
            return;
        }
        logging::InfoF("[EditorEncoder] using ffmpeg: {}", Narrow(g_ffmpegPath));

        HMODULE mf = LoadLibraryW(L"mfreadwrite.dll");
        void* target = mf ? (void*)GetProcAddress(mf, "MFCreateSinkWriterFromURL") : nullptr;
        if (!target)
        {
            logging::Error("[EditorEncoder] MFCreateSinkWriterFromURL not found");
            return;
        }

        MH_Initialize(); // harmless if already initialized elsewhere
        if (MH_CreateHook(target, (void*)&Hooked_Create, (void**)&g_origCreate) == MH_OK)
            MH_EnableHook(target);
        else
            logging::Error("[EditorEncoder] failed to hook MFCreateSinkWriterFromURL");
    }
}

static HookFunction hookEditorEncoder([]()
{
    EditorEncoder::Initialize();
});
