#include "test.hpp"

#include "wilfred/config/config.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/search/convert.hpp"
#include "wilfred/search/minis.hpp"

#include <cmath>
#include <vector>

void test_convert() {
  using namespace wilfred;

  // --- Format helpers ---
  {
    CHECK_EQ(normalize_format_token("MP3"), "mp3");
    CHECK_EQ(normalize_format_token(".WAV "), "wav");
    CHECK_EQ(normalize_format_token("JPEG"), "jpg");
    CHECK(is_audio_format("mp3"));
    CHECK(is_audio_format(".WAV"));
    CHECK(is_audio_format("ogg"));
    CHECK(is_audio_format("flac"));
    CHECK(!is_audio_format("png"));
    CHECK(is_image_format("png"));
    CHECK(is_image_format("jpg"));
    CHECK(is_image_format("bmp"));
    CHECK(is_image_format("tga"));
    CHECK(!is_image_format("mp3"));
    CHECK(is_known_convert_format("mp3"));
    CHECK(is_known_convert_format("png"));
    CHECK(!is_known_convert_format("docx"));
    CHECK_EQ(default_ext_for_format("mp3"), ".mp3");
    CHECK_EQ(default_ext_for_format("jpg"), ".jpg");
    CHECK(is_audio_convertible("song.MP3"));
    CHECK(is_audio_convertible("clip.wav"));
    CHECK(!is_audio_convertible("photo.png"));
    CHECK(is_image_convertible("photo.PNG"));
    CHECK(is_image_convertible("scan.bmp"));
    CHECK(!is_image_convertible("song.mp3"));
    CHECK(is_convertible("a.wav"));
    CHECK(is_convertible("b.png"));
    CHECK(!is_convertible("c.txt"));
    CHECK(!convert_install_hint("ffmpeg").empty());
    auto cmd = build_convert_ffmpeg_command("a.wav", "b.mp3", 44100, 2);
    CHECK(cmd.find("ffmpeg") != std::string::npos);
    CHECK(cmd.find("44100") != std::string::npos);
  }

  // --- WAV round-trip ---
  {
    WavData wav;
    wav.sample_rate = 44100;
    wav.channels = 1;
    wav.bits_per_sample = 16;
    wav.samples = {0.0f, 0.5f, -0.5f, 1.0f, -1.0f, 0.25f};
    std::vector<std::uint8_t> bytes;
    std::string err;
    CHECK(encode_wav_bytes(wav, 16, bytes, err));
    CHECK(!bytes.empty());
    WavData back;
    CHECK(decode_wav_bytes(bytes, back, err));
    CHECK_EQ(back.sample_rate, 44100);
    CHECK_EQ(back.channels, 1);
    CHECK_EQ(back.samples.size(), wav.samples.size());
    for (std::size_t i = 0; i < wav.samples.size(); ++i)
      CHECK(std::abs(back.samples[i] - wav.samples[i]) < 0.001);
    // 8/24/32-bit encode paths.
    for (int bits : {8, 24, 32}) {
      std::vector<std::uint8_t> enc;
      CHECK(encode_wav_bytes(wav, bits, enc, err));
      WavData dec;
      CHECK(decode_wav_bytes(enc, dec, err));
      CHECK_EQ(dec.samples.size(), wav.samples.size());
    }
    // Bad input rejected.
    WavData bad;
    std::vector<std::uint8_t> tiny = {1, 2, 3};
    CHECK(!decode_wav_bytes(tiny, bad, err));
    CHECK(!err.empty());
    CHECK(!encode_wav_bytes(wav, 7, bytes, err));
  }

  // --- Resample / remix ---
  {
    WavData wav;
    wav.sample_rate = 48000;
    wav.channels = 2;
    wav.bits_per_sample = 16;
    wav.samples.resize(480 * 2, 0.5f);
    auto down = resample_remix_wav(wav, 24000, 1);
    CHECK_EQ(down.sample_rate, 24000);
    CHECK_EQ(down.channels, 1);
    CHECK_EQ(down.samples.size(), 240u);
    auto same = resample_remix_wav(wav, 0, 0);
    CHECK_EQ(same.sample_rate, 48000);
    CHECK_EQ(same.channels, 2);
    WavData mono;
    mono.sample_rate = 44100;
    mono.channels = 1;
    mono.samples = {0.25f, 0.75f};
    auto stereo = resample_remix_wav(mono, 44100, 2);
    CHECK_EQ(stereo.samples.size(), 4u);
    CHECK(std::abs(stereo.samples[0] - stereo.samples[1]) < 1e-6);
  }

  // --- resolve_convert_output ---
  {
    std::string err;
    auto o1 = resolve_convert_output("/tmp/song.wav", "mp3", err);
    CHECK(o1.find(".mp3") != std::string::npos);
    auto o2 = resolve_convert_output("/tmp/song.wav", ".ogg", err);
    CHECK(o2.find(".ogg") != std::string::npos);
    auto o3 = resolve_convert_output("/tmp/song.wav", "out.flac", err);
    CHECK(o3.find("out.flac") != std::string::npos);
    CHECK(resolve_convert_output("/tmp/song.wav", "docx", err).empty());
    CHECK(!err.empty());
    CHECK(resolve_convert_output("", "mp3", err).empty());
  }

  // --- Image round-trips (native, no ffmpeg) ---
  {
    ImageRgba img;
    img.w = 3;
    img.h = 2;
    img.rgba = {
        255, 0,   0, 255, 0, 255, 0,   255, 0,   0, 255, 255,
        255, 255, 0, 255, 0, 255, 255, 255, 255, 0, 255, 128,
    };
    std::string err;
    std::vector<std::uint8_t> png;
    CHECK(encode_png_rgba(img, png, err));
    CHECK(png.size() > 8 && png[0] == 137);
    ImageRgba back;
    CHECK(decode_png_bytes(png.data(), png.size(), back, err));
    CHECK_EQ(back.w, 3);
    CHECK_EQ(back.h, 2);
    CHECK(back.rgba == img.rgba);

    std::vector<std::uint8_t> png_rgb;
    CHECK(encode_png_rgb(img, png_rgb, err));
    ImageRgba back_rgb;
    CHECK(decode_png_bytes(png_rgb.data(), png_rgb.size(), back_rgb, err));
    CHECK_EQ(back_rgb.w, 3);
    for (int i = 0; i < 6; ++i) {
      CHECK_EQ(back_rgb.rgba[static_cast<std::size_t>(i) * 4],
               img.rgba[static_cast<std::size_t>(i) * 4]);
      CHECK_EQ(back_rgb.rgba[static_cast<std::size_t>(i) * 4 + 1],
               img.rgba[static_cast<std::size_t>(i) * 4 + 1]);
      CHECK_EQ(back_rgb.rgba[static_cast<std::size_t>(i) * 4 + 2],
               img.rgba[static_cast<std::size_t>(i) * 4 + 2]);
    }

    std::vector<std::uint8_t> bmp;
    CHECK(encode_bmp_bytes(img, bmp));
    ImageRgba back_bmp;
    CHECK(decode_bmp_bytes(bmp.data(), bmp.size(), back_bmp, err));
    CHECK_EQ(back_bmp.w, 3);
    CHECK_EQ(back_bmp.h, 2);
    CHECK_EQ(back_bmp.rgba[0], 255);

    std::vector<std::uint8_t> ppm;
    CHECK(encode_ppm_bytes(img, ppm));
    ImageRgba back_ppm;
    CHECK(decode_ppm_bytes(ppm.data(), ppm.size(), back_ppm, err));
    CHECK_EQ(back_ppm.w, 3);

    std::vector<std::uint8_t> tga;
    CHECK(encode_tga_bytes(img, tga, err));
    ImageRgba back_tga;
    CHECK(decode_tga_bytes(tga.data(), tga.size(), back_tga, err));
    CHECK_EQ(back_tga.w, 3);
    CHECK(back_tga.rgba == img.rgba);

    // Generic dispatcher.
    ImageRgba via;
    CHECK(decode_image_bytes(png.data(), png.size(), via, err));
    CHECK_EQ(via.w, 3);
    CHECK(decode_image_bytes(bmp.data(), bmp.size(), via, err));
    CHECK_EQ(via.w, 3);
    std::vector<std::uint8_t> junk = {1, 2, 3, 4};
    CHECK(!decode_image_bytes(junk.data(), junk.size(), via, err));
  }

  // --- Background removal on a synthetic image ---
  {
    ImageRgba img;
    img.w = 6;
    img.h = 6;
    img.rgba.assign(6 * 6 * 4, 255);  // white bg
    for (int y = 2; y < 4; ++y)
      for (int x = 2; x < 4; ++x) {
        auto* p = &img.rgba[(static_cast<std::size_t>(y) * 6 + x) * 4];
        p[0] = 255;
        p[1] = 0;
        p[2] = 0;
      }
    BgRemoveOptions opts;
    opts.tolerance = 20;
    opts.contiguous = true;
    opts.feather = 0;
    ImageRgba cut;
    CHECK(remove_background(img, opts, cut));
    // Corners transparent, red center opaque.
    CHECK_EQ(cut.rgba[3], 0);
    auto* c = &cut.rgba[(static_cast<std::size_t>(2) * 6 + 2) * 4];
    CHECK_EQ(c[0], 255);
    CHECK_EQ(c[3], 255);
    // Explicit color + global mode.
    BgRemoveOptions g;
    g.r = 255;
    g.g = 255;
    g.b = 255;
    g.has_color = true;
    g.tolerance = 20;
    g.contiguous = false;
    ImageRgba cut2;
    CHECK(remove_background(img, g, cut2));
    CHECK_EQ(cut2.rgba[3], 0);
    // Hex parsing.
    std::uint8_t r = 0, gg = 0, b = 0;
    CHECK(parse_hex_color("#ff0000", r, gg, b));
    CHECK_EQ(r, 255);
    CHECK(parse_hex_color("#0f0", r, gg, b));
    CHECK_EQ(gg, 255);
    CHECK(!parse_hex_color("notacolor", r, gg, b));
  }

  // --- Query parsing ---
  {
    ConvertRequest q;
    CHECK(parse_convert_query("song.wav to mp3", q));
    CHECK_EQ(q.src, "song.wav");
    CHECK_EQ(q.fmt, "mp3");
    CHECK(parse_convert_query("a.wav to out.ogg", q));
    CHECK_EQ(q.dst, "out.ogg");
    CHECK(parse_convert_query("a.wav mp3", q));
    CHECK_EQ(q.fmt, "mp3");
    CHECK(parse_convert_query("a.wav -> flac", q));
    CHECK_EQ(q.fmt, "flac");
    CHECK(parse_convert_query("/tmp/a.wav to wav 44100 stereo", q));
    CHECK_EQ(q.sample_rate, 44100);
    CHECK_EQ(q.channels, 2);
    CHECK(!parse_convert_query("", q));
    CHECK(!parse_convert_query("song.wav to docx", q));
    // Source only: valid, no target yet.
    CHECK(parse_convert_query("song.wav", q));
    CHECK_EQ(q.src, "song.wav");
    CHECK(q.fmt.empty());

    BgRemoveRequest b;
    CHECK(parse_bgremove_query("photo.png", b));
    CHECK_EQ(b.src, "photo.png");
    CHECK(parse_bgremove_query("photo.png 40", b));
    CHECK_EQ(b.opts.tolerance, 40);
    CHECK(parse_bgremove_query("photo.png #ff0000 25", b));
    CHECK(b.opts.has_color);
    CHECK_EQ(b.opts.tolerance, 25);
    CHECK(parse_bgremove_query("photo.png white global", b));
    CHECK(!b.opts.contiguous);
    CHECK(!parse_bgremove_query("", b));

    std::string payload = encode_convert_payload("a.wav", "mp3", "a.mp3", 44100, 2);
    std::string src, fmt, dst;
    int rate = 0, ch = 0;
    CHECK(decode_convert_payload(payload, src, fmt, dst, rate, ch));
    CHECK_EQ(src, "a.wav");
    CHECK_EQ(fmt, "mp3");
    CHECK_EQ(rate, 44100);
    CHECK_EQ(ch, 2);

    BgRemoveOptions bo;
    bo.tolerance = 45;
    auto bp = encode_bgremove_payload("p.png", bo, "");
    std::string bs, bd;
    BgRemoveOptions bo2;
    CHECK(decode_bgremove_payload(bp, bs, bo2, bd));
    CHECK_EQ(bs, "p.png");
    CHECK_EQ(bo2.tolerance, 45);
  }

  // --- Mini intents + cards (no index, no ffmpeg execution) ---
  {
    CHECK(parse_mini_intent("convert song.wav to mp3").kind == MiniKind::Convert);
    CHECK(parse_mini_intent("transcode a.wav to ogg").kind == MiniKind::Convert);
    CHECK(parse_mini_intent("bgremove photo.png").kind == MiniKind::BgRemove);
    CHECK(parse_mini_intent("removebg photo.png").kind == MiniKind::BgRemove);
    CHECK(parse_mini_intent("transparent photo.png").kind == MiniKind::BgRemove);
    auto rm = parse_mini_intent("remove bg photo.png");
    CHECK(rm.kind == MiniKind::BgRemove);
    CHECK(rm.remainder.find("photo.png") != std::string::npos);
    Config cfg;
    auto usage = mini_results("convert", cfg, "");
    CHECK(!usage.empty());
    auto busage = mini_results("bgremove", cfg, "");
    CHECK(!busage.empty());
    auto bad = mini_results("convert song.wav to docx", cfg, "");
    CHECK(!bad.empty());
  }

  // --- File-level audio convert (native wav->wav) ---
  {
    WavData wav;
    wav.sample_rate = 44100;
    wav.channels = 1;
    wav.bits_per_sample = 16;
    wav.samples.assign(4410, 0.25f);
    std::vector<std::uint8_t> bytes;
    std::string err;
    CHECK(encode_wav_bytes(wav, 16, bytes, err));
    const std::string src = "convert-test-in.wav";
    CHECK(write_file_atomic(src, bytes.data(), bytes.size()));
    std::string out_path, cerr;
    CHECK(convert_audio_file(src, "convert-test-out.wav", 22050, 0, 0, out_path, cerr));
    CHECK(file_exists(out_path));
    std::string blob;
    CHECK(read_file_all(out_path, blob));
    std::vector<std::uint8_t> ob(blob.begin(), blob.end());
    WavData dec;
    CHECK(decode_wav_bytes(ob, dec, cerr));
    CHECK_EQ(dec.sample_rate, 22050);
    // Missing source fails cleanly.
    CHECK(!convert_audio_file("convert-test-missing-xyz.wav", "convert-test-out2.wav", 0, 0, 0,
                              out_path, cerr));
    CHECK(!cerr.empty());
    remove_file(src);
    remove_file("convert-test-out.wav");
    remove_file("convert-test-out2.wav");
  }

  // --- File-level image convert + bgremove (native) ---
  {
    ImageRgba img;
    img.w = 6;
    img.h = 6;
    img.rgba.assign(6 * 6 * 4, 255);
    for (int y = 2; y < 4; ++y)
      for (int x = 2; x < 4; ++x) {
        auto* p = &img.rgba[(static_cast<std::size_t>(y) * 6 + x) * 4];
        p[0] = 200;
        p[1] = 20;
        p[2] = 20;
      }
    std::vector<std::uint8_t> bmp;
    CHECK(encode_bmp_bytes(img, bmp));
    const std::string src = "convert-test-in.bmp";
    CHECK(write_file_atomic(src, bmp.data(), bmp.size()));
    std::string out_path, err;
    CHECK(convert_image_file(src, "convert-test-out.png", out_path, err));
    CHECK(file_exists(out_path));
    ImageRgba back;
    CHECK(decode_image_file(out_path, back, err));
    CHECK_EQ(back.w, 6);
    BgRemoveOptions opts;
    opts.tolerance = 25;
    std::string cut_path, cerr;
    CHECK(bgremove_file(src, "convert-test-cut.png", opts, cut_path, cerr));
    CHECK(file_exists(cut_path));
    ImageRgba cut;
    CHECK(decode_image_file(cut_path, cut, cerr));
    CHECK_EQ(cut.rgba[3], 0);  // corner transparent
    CHECK(!bgremove_file("convert-test-missing-xyz.png", "", opts, cut_path, cerr));
    remove_file(src);
    remove_file("convert-test-out.png");
    remove_file("convert-test-cut.png");
  }
}
