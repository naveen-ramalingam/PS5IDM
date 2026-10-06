// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Basic Test
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include "core/logger.h"
#include "platform/paths.h"
#include "downloader/segment.h"
#include "archive/archive_detector.h"
#include "hashing/checksum.h"
#include "settings/settings.h"

#include <cassert>
#include <iostream>
#include <string>

using namespace ps5dm;

static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name) \
    std::cout << "  TEST: " << #name << "... "; \
    try {

#define PASS() \
    std::cout << "PASS" << std::endl; testsPassed++; \
    } catch (const std::exception& e) { \
        std::cout << "FAIL: " << e.what() << std::endl; testsFailed++; \
    } catch (...) { \
        std::cout << "FAIL: unknown exception" << std::endl; testsFailed++; \
    }

#define ASSERT(cond) if (!(cond)) throw std::runtime_error("Assertion failed: " #cond)

// ─── Tests ──────────────────────────────────────────────────────────────────

void testFormatSize() {
    TEST(formatSize_bytes)
        ASSERT(formatSize(0) == "0 B");
        ASSERT(formatSize(512) == "512 B");
    PASS()

    TEST(formatSize_kilobytes)
        ASSERT(formatSize(1024) == "1.0 KB");
    PASS()

    TEST(formatSize_megabytes)
        auto result = formatSize(1024 * 1024);
        ASSERT(result == "1.0 MB");
    PASS()

    TEST(formatSize_gigabytes)
        ASSERT(formatSize(1024LL * 1024 * 1024) == "1.0 GB");
    PASS()

    TEST(formatSize_negative)
        ASSERT(formatSize(-1) == "0 B");
    PASS()
}

void testPaths() {
    TEST(Paths_join)
        ASSERT(Paths::join("/data", "file.txt") == "/data/file.txt");
        ASSERT(Paths::join("/data/", "file.txt") == "/data/file.txt");
    PASS()

    TEST(Paths_basename)
        ASSERT(Paths::basename("/data/downloads/file.txt") == "file.txt");
        ASSERT(Paths::basename("file.txt") == "file.txt");
    PASS()

    TEST(Paths_dirname)
        ASSERT(Paths::dirname("/data/downloads/file.txt") == "/data/downloads");
    PASS()

    TEST(Paths_extension)
        ASSERT(Paths::extension("file.txt") == ".txt");
        ASSERT(Paths::extension("file.tar.gz") == ".gz");
        ASSERT(Paths::extension("file") == "");
    PASS()

    TEST(Paths_stem)
        ASSERT(Paths::stem("file.txt") == "file");
    PASS()

    TEST(Paths_hasTraversal)
        ASSERT(Paths::hasTraversal("../etc/passwd") == true);
        ASSERT(Paths::hasTraversal("normal/path/file.txt") == false);
        ASSERT(Paths::hasTraversal("dir/../file") == true);
    PASS()
}

void testSegments() {
    TEST(SegmentScheduler_create)
        SegmentScheduler sched;
        sched.createSegments(100 * MB, 16 * MB);
        ASSERT(sched.segments().size() > 0);
        ASSERT(sched.totalSize() == 100 * MB);
        ASSERT(sched.allComplete() == false);
    PASS()

    TEST(SegmentScheduler_getNext)
        SegmentScheduler sched;
        sched.createSegments(32 * MB, 16 * MB);
        auto* seg = sched.getNextSegment();
        ASSERT(seg != nullptr);
        ASSERT(seg->status == SegmentStatus::DOWNLOADING);
        ASSERT(sched.activeCount() == 1);
    PASS()

    TEST(SegmentScheduler_complete)
        SegmentScheduler sched;
        sched.createSegments(16 * MB, 16 * MB);
        auto* seg = sched.getNextSegment();
        sched.completeSegment(seg->index);
        ASSERT(sched.completedCount() == 1);
        ASSERT(sched.allComplete() == true);
    PASS()
}

void testArchiveDetector() {
    TEST(ArchiveDetector_extension_zip)
        ASSERT(ArchiveDetector::detectByExtension("file.zip") == ArchiveType::ZIP);
    PASS()

    TEST(ArchiveDetector_extension_rar)
        ASSERT(ArchiveDetector::detectByExtension("file.rar") == ArchiveType::RAR);
    PASS()

    TEST(ArchiveDetector_extension_7z)
        ASSERT(ArchiveDetector::detectByExtension("file.7z") == ArchiveType::SEVENZIP);
    PASS()

    TEST(ArchiveDetector_multipart_rar)
        auto info = ArchiveDetector::parseMultipart("Game.part01.rar");
        ASSERT(info.type == ArchiveType::RAR);
        ASSERT(info.partNumber == 1);
        ASSERT(info.baseName == "Game");
    PASS()

    TEST(ArchiveDetector_multipart_7z)
        auto info = ArchiveDetector::parseMultipart("Backup.7z.001");
        ASSERT(info.type == ArchiveType::SEVENZIP);
        ASSERT(info.partNumber == 1);
        ASSERT(info.baseName == "Backup");
    PASS()

    TEST(ArchiveDetector_singleFile)
        auto info = ArchiveDetector::parseMultipart("archive.zip");
        ASSERT(info.partNumber == -1);
    PASS()
}

void testResult() {
    TEST(Result_success)
        auto r = Result<int>::success(42);
        ASSERT(r.ok() == true);
        ASSERT(r.get() == 42);
    PASS()

    TEST(Result_failure)
        auto r = Result<int>::failure(Error::make(1, "test error"));
        ASSERT(r.ok() == false);
        ASSERT(r.error.code == 1);
    PASS()

    TEST(Result_void_success)
        auto r = Result<void>::success();
        ASSERT(r.ok() == true);
    PASS()
}

void testError() {
    TEST(Error_none)
        auto e = Error::none();
        ASSERT(!e);
    PASS()

    TEST(Error_make)
        auto e = Error::make(42, "test");
        ASSERT(e);
        ASSERT(e.code == 42);
    PASS()

    TEST(Error_http)
        auto e = Error::http(503, "Service Unavailable");
        ASSERT(e.retryable == true);
        ASSERT(e.httpStatus == 503);
    PASS()

    TEST(Error_http_not_retryable)
        auto e = Error::http(404, "Not Found");
        ASSERT(e.retryable == false);
    PASS()
}

void testFormatDuration() {
    TEST(formatDuration_seconds)
        ASSERT(formatDuration(Seconds(45)) == "45s");
    PASS()

    TEST(formatDuration_minutes)
        auto result = formatDuration(Seconds(125));
        ASSERT(result == "2m 5s");
    PASS()

    TEST(formatDuration_hours)
        auto result = formatDuration(Seconds(3661));
        ASSERT(result == "1h 1m 1s");
    PASS()
}

void testDownloadCategory() {
    TEST(categorize_video)
        ASSERT(categorizeByExtension("movie.mp4") == DownloadCategory::VIDEOS);
    PASS()

    TEST(categorize_archive)
        ASSERT(categorizeByExtension("file.zip") == DownloadCategory::ARCHIVES);
    PASS()

    TEST(categorize_game)
        ASSERT(categorizeByExtension("game.pkg") == DownloadCategory::GAMES);
    PASS()

    TEST(categorize_unknown)
        ASSERT(categorizeByExtension("noext") == DownloadCategory::OTHER);
    PASS()
}

// ─── Main ───────────────────────────────────────────────────────────────────

int main() {
    std::cout << "╔═══════════════════════════════════════════╗" << std::endl;
    std::cout << "║    PS5 Download Manager - Unit Tests      ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;

    testResult();
    testError();
    testFormatSize();
    testFormatDuration();
    testDownloadCategory();
    testPaths();
    testSegments();
    testArchiveDetector();

    std::cout << std::endl;
    std::cout << "═══════════════════════════════════════════" << std::endl;
    std::cout << "  Passed: " << testsPassed << std::endl;
    std::cout << "  Failed: " << testsFailed << std::endl;
    std::cout << "═══════════════════════════════════════════" << std::endl;

    return testsFailed > 0 ? 1 : 0;
}
