#include "ocr_reader.h"
#include "xlsx_reader.h"   // 复用 SheetData

#include <windows.h>
#include <winrt/base.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>

using namespace winrt;
using namespace Windows::Media::Ocr;
using namespace Windows::Graphics::Imaging;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;

namespace winrt {
    using namespace Windows::Foundation;
}

bool OcrReader::isAvailable() {
    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        auto engine = OcrEngine::TryCreateFromUserProfileLanguages();
        return engine != nullptr;
    } catch (...) {
        return false;
    }
}

bool OcrReader::open(const std::string& filepath) {
    filename = filepath;
    sheets.clear();

    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);

        auto engine = OcrEngine::TryCreateFromUserProfileLanguages();
        if (!engine) return false;

        // 打开图片文件（UTF-8 → UTF-16 正确转换，中文路径必须走 MultiByteToWideChar）
        std::wstring wpath;
        {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, filepath.c_str(), (int)filepath.size(),
                                           nullptr, 0);
            if (wlen <= 0) return false;
            wpath.resize(wlen);
            MultiByteToWideChar(CP_UTF8, 0, filepath.c_str(), (int)filepath.size(),
                                &wpath[0], wlen);
        }
        auto file = StorageFile::GetFileFromPathAsync(wpath).get();
        if (!file) return false;

        auto stream = file.OpenAsync(FileAccessMode::Read).get();
        if (!stream) return false;

        // 解码为 SoftwareBitmap
        auto decoder = BitmapDecoder::CreateAsync(stream).get();
        if (!decoder) return false;

        auto bitmap = decoder.GetSoftwareBitmapAsync().get();
        if (!bitmap) return false;

        // 转为 OcrEngine 要求的格式（Bgra8 / Premultiplied）
        SoftwareBitmap ocrBitmap = SoftwareBitmap::Convert(bitmap,
            BitmapPixelFormat::Bgra8, BitmapAlphaMode::Premultiplied);

        // 识别
        auto result = engine.RecognizeAsync(ocrBitmap).get();
        if (!result) return false;

        // 逐行提取文本
        SheetData sd;
        sd.name = "OCR";
        sd.headers = { "内容" };
        int rowNum = 1;
        bool any = false;
        auto lines = result.Lines();
        for (uint32_t li = 0; li < lines.Size(); li++) {
            auto line = lines.GetAt(li);
            hstring text = line.Text();
            std::wstring ws(text.c_str());
            if (ws.empty()) continue;
            // 转 UTF-8
            int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
            if (len <= 0) continue;
            std::string utf8(len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), &utf8[0], len, nullptr, nullptr);
            sd.rows[rowNum][0] = utf8;
            rowNum++;
            any = true;
        }

        if (!any) return false;
        sheets.push_back(std::move(sd));
        return true;
    } catch (...) {
        return false;
    }
}
