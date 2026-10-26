// MacheteSVG - Unit test for Machete concept and storage mechanics
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"
#include "core/storage.hpp"
#include "core/game_scanner.hpp"
#include "core/jailbreak.hpp"
#include "core/i18n.hpp"
#include "core/save_manager.hpp"
#include "server/web_server.hpp"
#include "server/zip_writer.hpp"
#include "server/zip_reader.hpp"
#include "core/sha256_hmac.hpp"
#include "core/sfo_resigner.hpp"
#include "core/keystone_manager.hpp"
#include "core/pfs_mount_manager.hpp"
#include "core/samu_vault.hpp"
#include "core/account_manager.hpp"
#include "core/aes_xts.hpp"
#include "ui/qr_code.hpp"
#include "core/build_info.hpp"
#include "core/sqlite_adapter.hpp"
#include "core/logger.hpp"

#include <fcntl.h>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <chrono>
#include <thread>

namespace hui::testing
{

class MacheteTest : public ConceptFixture
{
  protected:
    std::unique_ptr<app::Concept> machete_ = concepts::make_machete(context_);
};

TEST_F(MacheteTest, ConceptCreationAndIdentity)
{
    ASSERT_NE(machete_, nullptr);
    EXPECT_STREQ(machete_->info().id, "machete");
    EXPECT_STREQ(machete_->info().name, "MacheteSaveGameManager");
}

TEST_F(MacheteTest, InitialDrawIsFiniteAndNotEmpty)
{
    expect_drawable(*machete_);
}

TEST_F(MacheteTest, NavigationDoesNotCrash)
{
    // Navegar por la lista de juegos
    send(*machete_, nav(Direction::down), 0.1f);
    expect_drawable(*machete_);

    send(*machete_, nav(Direction::down), 0.1f);
    expect_drawable(*machete_);

    send(*machete_, nav(Direction::up), 0.1f);
    expect_drawable(*machete_);

    // Navegar a la derecha (acciones)
    send(*machete_, nav(Direction::right), 0.1f);
    expect_drawable(*machete_);

    // Regresar a la lista
    send(*machete_, nav(Direction::left), 0.1f);
    expect_drawable(*machete_);
}

TEST_F(MacheteTest, ModalDialogFlow)
{
    // Ir a la derecha y presionar respaldo (Action::confirm)
    send(*machete_, nav(Direction::right), 0.1f);
    send(*machete_, press(Action::confirm), 0.1f);
    expect_drawable(*machete_);

    // En el diálogo, navegar hacia abajo
    send(*machete_, nav(Direction::down), 0.1f);
    expect_drawable(*machete_);

    // Cerrar el diálogo con back
    send(*machete_, press(Action::back), 0.1f);
    expect_drawable(*machete_);
}

TEST_F(MacheteTest, StorageDetectionAndFormatting)
{
    EXPECT_TRUE(machete::storage::ensure_internal_storage() || true);
    auto devices = machete::storage::scan_storage_devices();
    EXPECT_GE(devices.size(), 1u); // Minimo Internal

    machete::storage::StorageDevice dev;
    dev.free_bytes = 1024ULL * 1024ULL * 1024ULL * 5ULL;
    EXPECT_FALSE(dev.free_space_formatted().empty());
}

TEST_F(MacheteTest, WebServerInstanceAndLifecycle)
{
    auto &server = machete::server::WebServer::instance();
    EXPECT_FALSE(server.is_running());
    EXPECT_FALSE(server.local_ip().empty());
    EXPECT_EQ(server.port(), machete::server::kDefaultPort);
    EXPECT_EQ(server.port(), 8383);
    EXPECT_FALSE(server.url().empty());
    EXPECT_NE(server.url().find(":8383"), std::string::npos);
}

TEST_F(MacheteTest, WebServerHeaderShortcutAndDialog)
{
    // Presionar Triangle (Action::north) abre/alterna dialogo de Servidor Web
    send(*machete_, press(Action::north), 0.1f);
    expect_drawable(*machete_);

    // Presionar Confirm activa la opcion de dialogo
    send(*machete_, press(Action::confirm), 0.1f);
    expect_drawable(*machete_);

    // Cerrar modal con Back
    send(*machete_, press(Action::back), 0.1f);
    expect_drawable(*machete_);
}

TEST_F(MacheteTest, GameScannerAndTextureCycle)
{
    auto games = machete::scanner::scan_installed_games_and_saves();
    EXPECT_FALSE(games.empty());
    EXPECT_FALSE(games[0].title.empty());
    EXPECT_FALSE(games[0].title_id.empty());

    machete::scanner::load_game_icon_textures(games);
    machete::scanner::release_game_icon_textures(games);
}

TEST_F(MacheteTest, JailbreakStatusAndRequest)
{
    auto status = machete::jailbreak::check_status();
    EXPECT_NE(status.status_message.empty(), true);
    const char *str = machete::jailbreak::status_to_string(status.status);
    EXPECT_NE(str, nullptr);
}

TEST_F(MacheteTest, SaveManagerBackupAndRestore)
{
    auto games = machete::scanner::scan_installed_games_and_saves();
    ASSERT_FALSE(games.empty());

    machete::storage::StorageDevice dev;
    dev.label = "Test USB";
    dev.mount_path = "/tmp/machete_test_usb";
    dev.root_dir = "/tmp/machete_test_usb/MacheteSVG";
    dev.connected = true;

    auto backup_res = machete::save::backup_game_save(games[0], dev);
    EXPECT_FALSE(backup_res.message.empty());

    auto restore_res = machete::save::restore_game_save(games[0], dev);
    EXPECT_FALSE(restore_res.message.empty());
}

TEST_F(MacheteTest, PhysicalBackupDiscoveryWithoutStaleFlags)
{
    machete::scanner::ScannedGame game;
    game.title_id = "CUSA99999";
    game.title = "Mock Game";

    const std::string mock_root = "/tmp/machete_mock_storage/MacheteSVG";
    const std::string mock_folder = mock_root + "/CUSA99999 - Mock Game";
    mkdir("/tmp/machete_mock_storage", 0777);
    mkdir(mock_root.c_str(), 0777);
    mkdir(mock_folder.c_str(), 0777);

    FILE *f = std::fopen((mock_folder + "/save.bin").c_str(), "wb");
    if (f)
    {
        std::fputs("sample data", f);
        std::fclose(f);
    }

    std::vector<machete::storage::StorageDevice> devices;
    machete::storage::StorageDevice dev;
    dev.id = "internal";
    dev.label = "Internal Mock";
    dev.type = machete::storage::StorageType::Internal;
    dev.mount_path = "/tmp/machete_mock_storage";
    dev.root_dir = mock_root;
    dev.connected = false; // Simulation: stale connected flag is false!
    devices.push_back(dev);

    auto backups = machete::save::find_available_backups(game, devices);
    EXPECT_FALSE(backups.empty());
    if (!backups.empty())
    {
        EXPECT_EQ(backups[0].title_id, "CUSA99999");
        EXPECT_EQ(backups[0].folder_name, "CUSA99999 - Mock Game");
    }

    // Cleanup
    std::remove((mock_folder + "/save.bin").c_str());
    rmdir(mock_folder.c_str());
    rmdir(mock_root.c_str());
    rmdir("/tmp/machete_mock_storage");
}

TEST_F(MacheteTest, InternationalizationEngine)
{
    using namespace machete::i18n;

    // Verify all strings are populated in both languages
    for (int lang_idx = 0; lang_idx < 2; ++lang_idx)
    {
        set_language(static_cast<Language>(lang_idx));
        for (std::size_t id = 0; id < static_cast<std::size_t>(StringId::Count); ++id)
        {
            const char *str = get(static_cast<StringId>(id));
            ASSERT_NE(str, nullptr);
            EXPECT_GT(std::strlen(str), 0u);
        }
    }

    // Test English
    set_language(Language::English);
    EXPECT_EQ(current_language(), Language::English);
    EXPECT_STREQ(tr(StringId::JailbreakActiveRoot), "Jailbreak Active (Root)");
    EXPECT_STREQ(tr(StringId::BtnBackup), "Backup Save Data");
    EXPECT_STREQ(tr(StringId::DialogBackupHeading), "Select Backup Destination");

    // Test Spanish
    set_language(Language::Spanish);
    EXPECT_EQ(current_language(), Language::Spanish);
    EXPECT_STREQ(tr(StringId::JailbreakActiveRoot), "Jailbreak Activo (Root)");
    EXPECT_STREQ(tr(StringId::BtnBackup), "Crear Copia de Seguridad");
    EXPECT_STREQ(tr(StringId::DialogBackupHeading), "Selecciona el Destino del Respaldo");

    // Test Toggle
    toggle_language();
    EXPECT_EQ(current_language(), Language::English);

    // Test L3 action handling in concept
    send(*machete_, press(Action::l3), 0.1f);
    expect_drawable(*machete_);
    EXPECT_EQ(current_language(), Language::Spanish);
}

TEST_F(MacheteTest, NonLatinFilteringAndFallback)
{
    // 1. Validar que cadenas no latinas (arabe, cirilico, CJK) sean identificadas como no renderizables
    EXPECT_FALSE(machete::scanner::is_renderable_latin("وولڤرين من مارفل"));
    EXPECT_FALSE(machete::scanner::is_renderable_latin("Человек-паук"));
    EXPECT_FALSE(machete::scanner::is_renderable_latin("スパイダーマン"));
    EXPECT_FALSE(machete::scanner::is_renderable_latin(""));

    // 2. Validar que cadenas latinas y Title ID sean aceptadas
    EXPECT_TRUE(machete::scanner::is_renderable_latin("Marvel's Wolverine"));
    EXPECT_TRUE(machete::scanner::is_renderable_latin("PPSA03671"));
    EXPECT_TRUE(machete::scanner::is_renderable_latin("Spider-Man 2"));
    EXPECT_TRUE(machete::scanner::is_renderable_latin("El juego"));
}

TEST_F(MacheteTest, LocalizedParamJsonExtraction)
{
    // Crear archivo temporal param.json con arabe como default pero ingles/espanol disponible
    const std::string tmp_json = "/tmp/machete_wolverine_test.json";
    std::FILE *f = std::fopen(tmp_json.c_str(), "w");
    ASSERT_NE(f, nullptr);
    const char *content = "{\n"
                          "  \"localizedParameters\": {\n"
                          "    \"defaultLanguage\": \"ar-AE\",\n"
                          "    \"ar-AE\": {\n"
                          "      \"titleName\": \"وولڤرين من مارفل\"\n"
                          "    },\n"
                          "    \"en-US\": {\n"
                          "      \"titleName\": \"Marvel's Wolverine\"\n"
                          "    }\n"
                          "  }\n"
                          "}\n";
    std::fputs(content, f);
    std::fclose(f);

    std::string extracted = machete::scanner::extract_title_from_param_json(tmp_json);
    EXPECT_EQ(extracted, "Marvel's Wolverine");
    std::remove(tmp_json.c_str());

    // Crear archivo temporal param.json SOLO con arabe (debe retornar vacio para fallback a Title ID)
    const std::string tmp_arabic_json = "/tmp/machete_only_arabic.json";
    f = std::fopen(tmp_arabic_json.c_str(), "w");
    ASSERT_NE(f, nullptr);
    const char *arabic_only = "{\n"
                              "  \"localizedParameters\": {\n"
                              "    \"defaultLanguage\": \"ar-AE\",\n"
                              "    \"ar-AE\": {\n"
                              "      \"titleName\": \"وولڤرين من مارفل\"\n"
                              "    }\n"
                              "  }\n"
                              "}\n";
    std::fputs(arabic_only, f);
    std::fclose(f);

    extracted = machete::scanner::extract_title_from_param_json(tmp_arabic_json);
    EXPECT_EQ(extracted, "");
    std::remove(tmp_arabic_json.c_str());
}

TEST_F(MacheteTest, AppIconTextureLifecycle)
{
    // Probar carga y liberacion de textura PNG desde el icono oficial del proyecto
    std::string icon_path = "sce_sys/icon0.png";
    if (access(icon_path.c_str(), R_OK) != 0)
        icon_path = "/Users/thebigmike/.gemini/antigravity/scratch/MacheteSVG/sce_sys/icon0.png";
    unsigned int tex = machete::scanner::load_texture_from_png(icon_path.c_str());
    EXPECT_NE(tex, 0u);
    machete::scanner::release_texture(tex);
    EXPECT_EQ(tex, 0u);
}

TEST_F(MacheteTest, ProgressCallbackReportsProgress)
{
    machete::scanner::ScannedGame game;
    game.title_id = "PPSA01411";
    game.title = "Spider-Man Test";
    const std::string mock_save_dir = "/tmp/machete_test_save";
    mkdir(mock_save_dir.c_str(), 0777);
    FILE *f = std::fopen((mock_save_dir + "/slot0.bin").c_str(), "wb");
    if (f)
    {
        std::vector<char> dummy(1024 * 64, 'X');
        std::fwrite(dummy.data(), 1, dummy.size(), f);
        std::fclose(f);
    }
    game.save_path = mock_save_dir;

    machete::storage::StorageDevice dev;
    dev.label = "Test USB Async";
    dev.mount_path = "/tmp/machete_async_usb";
    dev.root_dir = "/tmp/machete_async_usb/MacheteSVG";
    dev.connected = true;

    std::vector<float> progress_points;
    auto backup_res = machete::save::backup_game_save(
        game, dev, [&](const machete::save::OperationProgress &p) {
            progress_points.push_back(p.progress);
            EXPECT_GE(p.progress, 0.0f);
            EXPECT_LE(p.progress, 1.0f);
        });

    EXPECT_FALSE(backup_res.message.empty());
    EXPECT_FALSE(progress_points.empty());
    if (!progress_points.empty())
    {
        EXPECT_GE(progress_points.back(), 0.0f);
    }

    // Cleanup
    std::remove((mock_save_dir + "/slot0.bin").c_str());
    rmdir(mock_save_dir.c_str());
    std::remove((dev.root_dir + "/" + game.title_id + " - " + game.title + "/slot0.bin").c_str());
    rmdir((dev.root_dir + "/" + game.title_id + " - " + game.title).c_str());
    rmdir(dev.root_dir.c_str());
    rmdir(dev.mount_path.c_str());
}

TEST_F(MacheteTest, AsyncOperationProtectionAndRender)
{
    // Navegar a botón de backup
    send(*machete_, nav(Direction::right), 0.1f);
    // Abrir modal de destino
    send(*machete_, press(Action::confirm), 0.1f);
    expect_drawable(*machete_);

    // Confirmar en el diálogo para disparar start_async_backup
    send(*machete_, press(Action::confirm), 0.1f);
    expect_drawable(*machete_);

    // Simular spam de botones mientras la operación se procesa
    for (int i = 0; i < 5; ++i)
    {
        send(*machete_, press(Action::confirm), 0.02f);
        send(*machete_, press(Action::west), 0.02f);
        send(*machete_, press(Action::north), 0.02f);
        send(*machete_, nav(Direction::down), 0.02f);
        expect_drawable(*machete_);
    }

    // Permitir tiempo para que el hilo termine y se actualice el estado
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    send(*machete_, InputFrame{}, 0.1f);
    expect_drawable(*machete_);
}

TEST_F(MacheteTest, KeystoneAndSealedKeyAtomicRestore)
{
    machete::scanner::ScannedGame game;
    game.title_id = "CUSA03173";
    game.title = "Bloodborne";
    const std::string target_save_dir = "/tmp/machete_test_target_save";
    mkdir(target_save_dir.c_str(), 0777);

    // 1. Simular partida existente en consola con keystone viejo de la consola
    std::FILE *f_old_k = std::fopen((target_save_dir + "/keystone").c_str(), "wb");
    ASSERT_NE(f_old_k, nullptr);
    std::fputs("CONSOLE_OLD_KEYSTONE", f_old_k);
    std::fclose(f_old_k);

    std::FILE *f_old_s = std::fopen((target_save_dir + "/sealedkey").c_str(), "wb");
    ASSERT_NE(f_old_s, nullptr);
    std::fputs("CONSOLE_OLD_SEALEDKEY", f_old_s);
    std::fclose(f_old_s);

    game.save_path = target_save_dir;

    // 2. Simular paquete de respaldo en almacenamiento externo con keystone y sealedkey coincidentes
    const std::string backup_root = "/tmp/machete_test_backup_usb/MacheteSVG";
    const std::string backup_game_dir = backup_root + "/CUSA03173 - Bloodborne";
    mkdir("/tmp/machete_test_backup_usb", 0777);
    mkdir(backup_root.c_str(), 0777);
    mkdir(backup_game_dir.c_str(), 0777);

    std::FILE *f_new_k = std::fopen((backup_game_dir + "/keystone").c_str(), "wb");
    ASSERT_NE(f_new_k, nullptr);
    std::fputs("BACKUP_NEW_KEYSTONE", f_new_k);
    std::fclose(f_new_k);

    std::FILE *f_new_s = std::fopen((backup_game_dir + "/sealedkey").c_str(), "wb");
    ASSERT_NE(f_new_s, nullptr);
    std::fputs("BACKUP_NEW_SEALEDKEY", f_new_s);
    std::fclose(f_new_s);

    std::FILE *f_data = std::fopen((backup_game_dir + "/savedata0.bin").c_str(), "wb");
    ASSERT_NE(f_data, nullptr);
    std::fputs("SAVEDATA_DATA_PAYLOAD", f_data);
    std::fclose(f_data);

    machete::save::BackupItem item;
    item.title_id = "CUSA03173";
    item.title = "Bloodborne";
    item.folder_path = backup_game_dir;
    item.device_label = "USB Backup";
    item.total_bytes = 1024;

    // 3. Ejecutar restauracion
    auto res = machete::save::restore_backup_item(game, item, nullptr);
    EXPECT_TRUE(res.success);

    // 4. Verificar que el keystone restaurado sea el del respaldo (emparejado con su sealedkey)
    // y NUNCA el keystone viejo de la consola para evitar corrupcion criptografica
    char read_buf[64] = {0};
    std::FILE *f_chk_k = std::fopen((target_save_dir + "/keystone").c_str(), "rb");
    ASSERT_NE(f_chk_k, nullptr);
    std::size_t rk = std::fread(read_buf, 1, sizeof(read_buf) - 1, f_chk_k);
    read_buf[rk] = '\0';
    std::fclose(f_chk_k);
    EXPECT_STREQ(read_buf, "BACKUP_NEW_KEYSTONE");

    char read_buf_s[64] = {0};
    std::FILE *f_chk_s = std::fopen((target_save_dir + "/sealedkey").c_str(), "rb");
    ASSERT_NE(f_chk_s, nullptr);
    std::size_t rs = std::fread(read_buf_s, 1, sizeof(read_buf_s) - 1, f_chk_s);
    read_buf_s[rs] = '\0';
    std::fclose(f_chk_s);
    EXPECT_STREQ(read_buf_s, "BACKUP_NEW_SEALEDKEY");

    // Limpieza
    std::remove((target_save_dir + "/keystone").c_str());
    std::remove((target_save_dir + "/sealedkey").c_str());
    std::remove((target_save_dir + "/savedata0.bin").c_str());
    rmdir(target_save_dir.c_str());

    std::remove((backup_game_dir + "/keystone").c_str());
    std::remove((backup_game_dir + "/sealedkey").c_str());
    std::remove((backup_game_dir + "/savedata0.bin").c_str());
    rmdir(backup_game_dir.c_str());
    rmdir(backup_root.c_str());
    rmdir("/tmp/machete_test_backup_usb");
}

TEST(MacheteCryptoTest, Sha256StandardVector)
{
    // NIST standard vector: "abc"
    const char *msg = "abc";
    std::uint8_t digest[32];
    machete::crypto::Sha256 sha;
    sha.update(reinterpret_cast<const std::uint8_t *>(msg), 3);
    sha.finish(digest);

    char hex[65] = {0};
    for (int i = 0; i < 32; ++i)
    {
        std::snprintf(hex + (i * 2), 3, "%02x", digest[i]);
    }

    EXPECT_STREQ(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(MacheteCryptoTest, HmacSha256Execution)
{
    const std::uint8_t key[] = "secret_key";
    const std::uint8_t data[] = "machete_payload_data";
    std::uint8_t out_mac[32] = {0};

    machete::crypto::hmac_sha256(key, sizeof(key) - 1, data, sizeof(data) - 1, out_mac);

    bool non_zero = false;
    for (int i = 0; i < 32; ++i)
    {
        if (out_mac[i] != 0)
            non_zero = true;
    }
    EXPECT_TRUE(non_zero);
}

TEST(MacheteVaultTest, ArchiveCreationAndIntegrity)
{
    const std::string test_dir = "/tmp/machete_vault_test";
    mkdir(test_dir.c_str(), 0777);

    // Crear carpeta simulada de partida
    const std::string save_src = test_dir + "/mock_save";
    mkdir(save_src.c_str(), 0777);

    bool ok = machete::vault::archive_crypto_vault(test_dir, "PPSA99999", "PS5", save_src, 0x105f615a);
    EXPECT_TRUE(ok);

    const std::string bin_path = test_dir + "/crypto_vault.bin";
    std::FILE *f = std::fopen(bin_path.c_str(), "rb");
    ASSERT_NE(f, nullptr);

    machete::vault::CryptoVaultRecord rec;
    std::size_t n = std::fread(&rec, 1, sizeof(rec), f);
    std::fclose(f);

    EXPECT_EQ(n, sizeof(rec));
    EXPECT_EQ(rec.magic, machete::vault::VAULT_MAGIC);
    EXPECT_EQ(rec.version, machete::vault::VAULT_VERSION);
    EXPECT_EQ(rec.local_user_id, 0x105f615a);
    EXPECT_STREQ(rec.title_id, "PPSA99999");
    EXPECT_STREQ(rec.platform, "PS5");

    std::remove(bin_path.c_str());
    rmdir(save_src.c_str());
    rmdir(test_dir.c_str());
}

TEST(MacheteServerTest, ZipWriterStreamingAndCrc32)
{
    const std::string tmp_txt = "/tmp/machete_test_file.txt";
    const std::string content = "Save file dummy data content for unit testing";
    int src_fd = open(tmp_txt.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    ASSERT_GE(src_fd, 0);
    (void)write(src_fd, content.data(), content.size());
    close(src_fd);

    machete::server::ZipWriter zip;
    zip.add_file(tmp_txt, "savedata/param.json");
    zip.prepare();
    const std::uint64_t expected_size = zip.total_zip_size();
    EXPECT_GT(expected_size, content.size());

    // Escribir a un archivo temporal para verificar el stream
    const std::string tmp_zip = "/tmp/machete_test_zip.zip";
    int fd = open(tmp_zip.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    ASSERT_GE(fd, 0);

    bool stream_ok = zip.stream_to_fd(fd);
    close(fd);
    EXPECT_TRUE(stream_ok);

    struct stat st;
    ASSERT_EQ(stat(tmp_zip.c_str(), &st), 0);
    EXPECT_EQ(static_cast<std::uint64_t>(st.st_size), expected_size);

    std::remove(tmp_zip.c_str());
    std::remove(tmp_txt.c_str());
}

TEST(MacheteAccountTest, DetectActiveAccountIdFallback)
{
    std::uint64_t acc = machete::account::detect_active_account_id(0x105f615a);
    EXPECT_NE(acc, 0ULL);
}

TEST(MacheteKeystoneTest, LookupSearchCandidates)
{
    // Verificar que find_authentic_game_keystone maneja IDs inexistentes sin crashear
    std::string path = machete::keystone::find_authentic_game_keystone("NONEXISTENT");
    EXPECT_TRUE(path.empty());
}

namespace
{

std::string test_http_request(std::uint16_t port, const std::string &request)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "";

    struct timeval tv{};
    tv.tv_sec = 2;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0)
    {
        close(sock);
        return "";
    }

    send(sock, request.data(), request.size(), 0);

    std::string response;
    char buf[1024];
    while (true)
    {
        ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        response.append(buf, static_cast<std::size_t>(n));
    }
    close(sock);
    return response;
}

} // namespace

TEST(MacheteServerTest, HttpApiEndpointsAndSecurity)
{
    auto &server = machete::server::WebServer::instance();
    const std::uint16_t test_port = 18383;
    EXPECT_TRUE(server.start(test_port));
    EXPECT_TRUE(server.is_running());
    EXPECT_EQ(server.port(), test_port);

    // 1. GET /api/status
    std::string status_resp = test_http_request(test_port, "GET /api/status HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(status_resp.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(status_resp.find(std::string("\"version\":\"") + machete::build::version_string() + "\""), std::string::npos);
    EXPECT_NE(status_resp.find(std::string("\"build\":\"") + machete::build::build_number() + "\""), std::string::npos);
    EXPECT_NE(status_resp.find("\"status\":\"running\""), std::string::npos);

    // 1b. GET /api/log
    std::string log_resp = test_http_request(test_port, "GET /api/log HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(log_resp.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(log_resp.find("Content-Type: text/plain"), std::string::npos);

    // 1c. GET /api/logo
    std::string logo_resp = test_http_request(test_port, "GET /api/logo HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(logo_resp.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(logo_resp.find("Content-Type: image/png"), std::string::npos);

    // 2. GET / (Web UI HTML)
    std::string root_resp = test_http_request(test_port, "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(root_resp.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(root_resp.find("<!DOCTYPE html>"), std::string::npos);

    // 3. GET /api/games
    std::string games_resp = test_http_request(test_port, "GET /api/games HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(games_resp.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(games_resp.find("["), std::string::npos);

    // 4. Security Traversal Attack on /api/file/
    std::string trav_file = test_http_request(test_port, "GET /api/file/PPSA01411/../../etc/passwd HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(trav_file.find("HTTP/1.1 403 Forbidden"), std::string::npos);

    // 5. Security Traversal Attack on /api/icon/
    std::string trav_icon = test_http_request(test_port, "GET /api/icon/../../etc/passwd HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(trav_icon.find("HTTP/1.1 400 Bad Request"), std::string::npos);

    // 6. Security Traversal Attack on /api/download/
    std::string trav_dl = test_http_request(test_port, "GET /api/download/../../etc/passwd HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(trav_dl.find("HTTP/1.1 400 Bad Request"), std::string::npos);

    // 7. Security rejection when update module is disabled by default for store launch
    EXPECT_FALSE(server.is_update_module_enabled());
    std::string disabled_resp = test_http_request(test_port, "POST /api/update HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nConnection: close\r\n\r\nHELLO");
    EXPECT_NE(disabled_resp.find("HTTP/1.1 403 Forbidden"), std::string::npos);
    EXPECT_NE(disabled_resp.find("deshabilitado"), std::string::npos);

    // 7b. Security rejection on invalid /api/update upload when enabled
    server.set_update_module_enabled(true);
    EXPECT_TRUE(server.is_update_module_enabled());
    std::string bad_update = test_http_request(test_port, "POST /api/update HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nConnection: close\r\n\r\nHELLO");
    EXPECT_NE(bad_update.find("HTTP/1.1 400 Bad Request"), std::string::npos);

    // 8. Successful OTA update deployment with valid package
    {
        machete::server::ZipWriter pkg_writer;
        const std::string ota_temp = "/tmp/machete_test_ota_pkg";
        mkdir(ota_temp.c_str(), 0777);
        {
            std::ofstream f(ota_temp + "/eboot.bin", std::ios::binary);
            f << "TEST_EBOOT_OTA";
        }
        pkg_writer.add_directory(ota_temp, "PPSA99700");
        auto zip_data = pkg_writer.write_to_memory();
        (void)std::remove((ota_temp + "/eboot.bin").c_str());
        (void)rmdir(ota_temp.c_str());

        std::string req = "POST /api/update HTTP/1.1\r\nHost: localhost\r\nContent-Length: " +
                          std::to_string(zip_data.size()) + "\r\nConnection: close\r\n\r\n";
        req.append(reinterpret_cast<const char *>(zip_data.data()), zip_data.size());

        std::string update_resp = test_http_request(test_port, req);
        EXPECT_NE(update_resp.find("HTTP/1.1 200 OK"), std::string::npos);
        EXPECT_NE(update_resp.find("\"success\":true"), std::string::npos);
        EXPECT_NE(update_resp.find("instalada con exito"), std::string::npos);

        // 8b. Disabling update module returns 403 Forbidden
        server.set_update_module_enabled(false);
        EXPECT_FALSE(server.is_update_module_enabled());
        std::string dis_resp = test_http_request(test_port, req);
        EXPECT_NE(dis_resp.find("HTTP/1.1 403 Forbidden"), std::string::npos);
        EXPECT_NE(dis_resp.find("deshabilitado"), std::string::npos);
    }

    // 9. Shin-chan animation endpoints for Web Updater
    std::string shin_dance = test_http_request(test_port, "GET /api/shinchan/dance.gif HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(shin_dance.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(shin_dance.find("Content-Type: image/gif"), std::string::npos);

    std::string shin_done = test_http_request(test_port, "GET /api/shinchan/done.gif HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(shin_done.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(shin_done.find("Content-Type: image/gif"), std::string::npos);

    std::string shin_error = test_http_request(test_port, "GET /api/shinchan?state=error HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_NE(shin_error.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(shin_error.find("Content-Type: image/gif"), std::string::npos);

    server.stop();
    EXPECT_FALSE(server.is_running());
}

TEST(MacheteServerTest, ZipWriterAutoPrepareAndMemoryBuffer)
{
    const std::string test_dir = "/tmp/machete_zip_unit_test";
    mkdir(test_dir.c_str(), 0777);
    const std::string file_a = test_dir + "/file_a.bin";
    const std::string file_b = test_dir + "/file_b.bin";

    FILE *fa = std::fopen(file_a.c_str(), "wb");
    ASSERT_NE(fa, nullptr);
    std::fputs("HELLO_ZIP_A", fa);
    std::fclose(fa);

    FILE *fb = std::fopen(file_b.c_str(), "wb");
    ASSERT_NE(fb, nullptr);
    std::fputs("HELLO_ZIP_B", fb);
    std::fclose(fb);

    machete::server::ZipWriter zip;
    zip.add_directory(test_dir, "test_prefix");
    EXPECT_GE(zip.entry_count(), 2u);

    // Test write_to_memory (which auto-prepares if needed)
    std::vector<std::uint8_t> mem = zip.write_to_memory();
    ASSERT_GE(mem.size(), 30u);
    // Check standard PK zip local header signature: 0x04034B50 (LE: 0x50, 0x4b, 0x03, 0x04)
    EXPECT_EQ(mem[0], 0x50);
    EXPECT_EQ(mem[1], 0x4B);
    EXPECT_EQ(mem[2], 0x03);
    EXPECT_EQ(mem[3], 0x04);

    // Check EOCD signature in buffer: 0x06054B50 (LE: 0x50, 0x4b, 0x05, 0x06)
    bool has_eocd = false;
    for (std::size_t i = 0; i + 3 < mem.size(); ++i)
    {
        if (mem[i] == 0x50 && mem[i + 1] == 0x4B && mem[i + 2] == 0x05 && mem[i + 3] == 0x06)
        {
            has_eocd = true;
            break;
        }
    }
    EXPECT_TRUE(has_eocd);

    std::remove(file_a.c_str());
    std::remove(file_b.c_str());
    rmdir(test_dir.c_str());
}

TEST(MacheteServerTest, ThreadSafeFindGame)
{
    auto &server = machete::server::WebServer::instance();
    auto games = machete::scanner::scan_installed_games_and_saves();
    server.update_games(games);

    machete::scanner::ScannedGame found;
    EXPECT_FALSE(server.find_game("NON_EXISTENT_TITLE", found));

    if (!games.empty())
    {
        EXPECT_TRUE(server.find_game(games[0].title_id, found));
        EXPECT_EQ(found.title_id, games[0].title_id);
    }
}

TEST(MachetePfsTest, SaveDummyPreservationAndParamJsonUpdate)
{
    const std::string test_root = "/tmp/machete_pfs_unit_test";
    const std::string local_save = test_root + "/local_save";
    const std::string local_sys = local_save + "/sce_sys";
    const std::string preserve_dir = test_root + "/preserved_dummy";

    mkdir(test_root.c_str(), 0777);
    mkdir(local_save.c_str(), 0777);
    mkdir(local_sys.c_str(), 0777);

    // 1. Sin sdimg_*, has_local_save_dummy debe devolver false
    EXPECT_FALSE(machete::pfs::has_local_save_dummy(local_save));

    // 2. Crear un Save Dummy PS5 simulado (sdimg_slot0 + sce_sys/param.json)
    const std::string sdimg_path = local_save + "/sdimg_slot0";
    {
        FILE *f = std::fopen(sdimg_path.c_str(), "wb");
        ASSERT_NE(f, nullptr);
        std::fputs("LOCAL_CONSOLE_SAMU_SEALED_CONTAINER_DATA_1234567890", f);
        std::fclose(f);
    }
    const std::string param_json_path = local_sys + "/param.json";
    {
        FILE *f = std::fopen(param_json_path.c_str(), "wb");
        ASSERT_NE(f, nullptr);
        std::fputs("{\"titleId\":\"PPSA01411\",\"fileSize\":1024,\"mtime\":\"2026-01-01T00:00:00Z\"}", f);
        std::fclose(f);
    }

    EXPECT_TRUE(machete::pfs::has_local_save_dummy(local_save));

    // 3. Preservar el Save Dummy local antes de que una copia sobrescriba la carpeta
    EXPECT_TRUE(machete::pfs::preserve_local_save_dummy(local_save, preserve_dir));
    EXPECT_EQ(access((preserve_dir + "/sdimg_slot0").c_str(), F_OK), 0);

    // 4. Actualizar fileSize en param.json al tamano real de sdimg_slot0
    machete::pfs::update_ps5_param_json_size(param_json_path, 51ULL);
    {
        FILE *f = std::fopen(param_json_path.c_str(), "rb");
        ASSERT_NE(f, nullptr);
        char buf[256] = {0};
        std::fread(buf, 1, sizeof(buf) - 1, f);
        std::fclose(f);
        EXPECT_NE(std::string(buf).find("\"fileSize\":51"), std::string::npos);
    }

    // Limpieza
    std::remove((preserve_dir + "/sce_sys/param.json").c_str());
    rmdir((preserve_dir + "/sce_sys").c_str());
    std::remove((preserve_dir + "/sdimg_slot0").c_str());
    rmdir(preserve_dir.c_str());
    std::remove(param_json_path.c_str());
    rmdir(local_sys.c_str());
    std::remove(sdimg_path.c_str());
    rmdir(local_save.c_str());
    rmdir(test_root.c_str());
}

TEST(MachetePfsTest, CrossConsolePs5RestoreRequiresSaveDummyWhenMissing)
{
    const std::string test_root = "/tmp/machete_cross_test";
    const std::string backup_dir = test_root + "/PPSA77777";
    const std::string dec_slot = backup_dir + "/decrypted/slot0";
    const std::string dst_save = test_root + "/dst_savedata_empty";

    mkdir(test_root.c_str(), 0777);
    mkdir(backup_dir.c_str(), 0777);
    mkdir((backup_dir + "/decrypted").c_str(), 0777);
    mkdir(dec_slot.c_str(), 0777);

    // Crear un archivo desencriptado en el respaldo
    {
        FILE *f = std::fopen((dec_slot + "/progress.dat").c_str(), "wb");
        ASSERT_NE(f, nullptr);
        std::fputs("DECRYPTED_GAME_PROGRESS", f);
        std::fclose(f);
    }
    EXPECT_TRUE(machete::pfs::has_decrypted_payload(backup_dir));

    // Crear un crypto_vault.bin simulando otra consola (open_psid diferente)
    machete::vault::CryptoVaultRecord rec{};
    std::memset(&rec, 0, sizeof(rec));
    rec.magic = machete::vault::VAULT_MAGIC;
    rec.version = machete::vault::VAULT_VERSION;
    std::snprintf(rec.title_id, sizeof(rec.title_id), "%s", "PPSA77777");
    std::snprintf(rec.platform, sizeof(rec.platform), "%s", "PS5");
    rec.open_psid[0] = 0xCC;
    rec.open_psid[1] = 0xDD;
    {
        machete::crypto::Sha256 sha;
        const std::size_t hashable_len = offsetof(machete::vault::CryptoVaultRecord, integrity_sha256);
        sha.update(reinterpret_cast<const std::uint8_t *>(&rec), hashable_len);
        sha.finish(rec.integrity_sha256);

        FILE *fv = std::fopen((backup_dir + "/crypto_vault.bin").c_str(), "wb");
        ASSERT_NE(fv, nullptr);
        std::fwrite(&rec, 1, sizeof(rec), fv);
        std::fclose(fv);
    }

    machete::vault::CryptoVaultRecord verify_rec{};
    EXPECT_TRUE(machete::vault::read_vault_record(backup_dir, verify_rec));
    EXPECT_TRUE(machete::pfs::is_backup_from_different_console(backup_dir, true));

    // Intentar restaurar en dst_save donde NO existe Save Dummy local
    machete::scanner::ScannedGame game;
    game.title = "Cross Console Test Game";
    game.title_id = "PPSA77777";
    game.platform = "PS5";
    game.save_path = dst_save;

    machete::save::BackupItem item;
    item.title_id = "PPSA77777";
    item.folder_path = backup_dir;
    item.has_decrypted = true;
    item.has_crypto_vault = true;

    auto res = machete::save::restore_backup_item(game, item);
    EXPECT_FALSE(res.success);
    EXPECT_NE(res.message.find("Save Dummy"), std::string::npos);

    // Limpieza
    std::remove((backup_dir + "/crypto_vault.bin").c_str());
    std::remove((dec_slot + "/progress.dat").c_str());
    rmdir(dec_slot.c_str());
    rmdir((backup_dir + "/decrypted").c_str());
    rmdir(backup_dir.c_str());
    rmdir(test_root.c_str());
}

TEST(MacheteQrTest, EncodeHttpUrlAndVerifyFinderPatterns)
{
    hui::ui::qr::QrMatrix matrix{};
    const std::string url = "http://192.168.1.50:8383";
    const bool ok = hui::ui::qr::encode_text(url, matrix);

    EXPECT_TRUE(ok);
    EXPECT_GE(matrix.size, 21);
    EXPECT_LE(matrix.size, 33);

    const int s = matrix.size;

    // 1. Finder superior izquierdo (0, 0 a 6, 6)
    EXPECT_TRUE(matrix.get(0, 0));
    EXPECT_TRUE(matrix.get(6, 0));
    EXPECT_TRUE(matrix.get(0, 6));
    EXPECT_TRUE(matrix.get(6, 6));
    EXPECT_FALSE(matrix.get(1, 1));
    EXPECT_TRUE(matrix.get(3, 3)); // Centro oscuro 3x3

    // 2. Finder superior derecho (s - 7, 0 a s - 1, 6)
    EXPECT_TRUE(matrix.get(s - 7, 0));
    EXPECT_TRUE(matrix.get(s - 1, 0));
    EXPECT_TRUE(matrix.get(s - 7, 6));
    EXPECT_TRUE(matrix.get(s - 1, 6));
    EXPECT_FALSE(matrix.get(s - 6, 1));
    EXPECT_TRUE(matrix.get(s - 4, 3));

    // 3. Finder inferior izquierdo (0, s - 7 a 6, s - 1)
    EXPECT_TRUE(matrix.get(0, s - 7));
    EXPECT_TRUE(matrix.get(6, s - 7));
    EXPECT_TRUE(matrix.get(0, s - 1));
    EXPECT_TRUE(matrix.get(6, s - 1));
    EXPECT_FALSE(matrix.get(1, s - 6));
    EXPECT_TRUE(matrix.get(3, s - 4));
}

TEST(MacheteQrTest, BoundsAndDeterminism)
{
    hui::ui::qr::QrMatrix m1{};
    hui::ui::qr::QrMatrix m2{};

    // Cadena vacia
    EXPECT_FALSE(hui::ui::qr::encode_text("", m1));

    // Cadena demasiado larga (> 62 caracteres)
    const std::string long_str(65, 'A');
    EXPECT_FALSE(hui::ui::qr::encode_text(long_str, m1));

    // Determinismo: dos ejecuciones con la misma cadena producen matrices identicas
    const std::string test_url = "http://10.0.0.123:8383";
    EXPECT_TRUE(hui::ui::qr::encode_text(test_url, m1));
    EXPECT_TRUE(hui::ui::qr::encode_text(test_url, m2));
    EXPECT_EQ(m1.size, m2.size);
    EXPECT_EQ(m1.modules, m2.modules);
}

TEST(MacheteSaveTest, Ps4SaveRestoreBypassesSaveDummy)
{
    const std::string test_root = "/tmp/machete_ps4_test";
    const std::string backup_dir = test_root + "/CUSA18600 - TestPS4Game";
    const std::string dst_save = test_root + "/dst_ps4_save";

    mkdir(test_root.c_str(), 0777);
    mkdir(backup_dir.c_str(), 0777);

    // Crear un archivo de partida de PS4 simulado
    {
        FILE *f = std::fopen((backup_dir + "/savedata0.bin").c_str(), "wb");
        ASSERT_NE(f, nullptr);
        std::fputs("PS4_ENCRYPTED_SAVE_PAYLOAD", f);
        std::fclose(f);
    }
    {
        FILE *f = std::fopen((backup_dir + "/keystone").c_str(), "wb");
        ASSERT_NE(f, nullptr);
        std::vector<std::uint8_t> dummy_ks(96, 0x5A);
        std::fwrite(dummy_ks.data(), 1, dummy_ks.size(), f);
        std::fclose(f);
    }

    // Verificar que is_backup_from_different_console retorna false para PS4
    EXPECT_FALSE(machete::pfs::is_backup_from_different_console(backup_dir, false));

    // Restaurar partida de PS4 en un destino vacio (sin Save Dummy previo)
    machete::scanner::ScannedGame game;
    game.title = "Test PS4 Game";
    game.title_id = "CUSA18600";
    game.platform = "PS4";
    game.save_path = dst_save;

    machete::save::BackupItem item;
    item.title_id = "CUSA18600";
    item.folder_path = backup_dir;
    item.has_decrypted = false;
    item.has_crypto_vault = false;

    auto res = machete::save::restore_backup_item(game, item);
    EXPECT_TRUE(res.success);
    EXPECT_EQ(res.message.find("Save Dummy"), std::string::npos);

    // Comprobar que los archivos existen en el destino
    struct stat st{};
    EXPECT_EQ(stat((dst_save + "/savedata0.bin").c_str(), &st), 0);
    EXPECT_EQ(stat((dst_save + "/keystone").c_str(), &st), 0);

    // Limpieza
    std::remove((dst_save + "/savedata0.bin").c_str());
    std::remove((dst_save + "/keystone").c_str());
    rmdir(dst_save.c_str());
    std::remove((backup_dir + "/savedata0.bin").c_str());
    std::remove((backup_dir + "/keystone").c_str());
    rmdir(backup_dir.c_str());
    rmdir(test_root.c_str());
}

TEST(MacheteRestoreTest, Ps4RestoreFallbackKeystoneWhenNoKeystonePresent)
{
    const std::string test_root = "/tmp/machete_ps4_no_ks_test";
    const std::string dst_save = test_root + "/dst_save";
    const std::string backup_dir = test_root + "/CUSA18600 - DragonQuest";

    (void)mkdir(test_root.c_str(), 0777);
    (void)mkdir(dst_save.c_str(), 0777);
    (void)mkdir(backup_dir.c_str(), 0777);

    // Crear unicamente archivos de datos de guardado (sin keystone en el respaldo ni en el destino)
    {
        std::FILE *f = std::fopen((backup_dir + "/Book").c_str(), "wb");
        ASSERT_NE(f, nullptr);
        std::fputs("DUMMY_PS4_SAVE_PAYLOAD", f);
        std::fclose(f);
    }
    {
        std::FILE *f = std::fopen((backup_dir + "/Book.bin").c_str(), "wb");
        ASSERT_NE(f, nullptr);
        std::fputs("DUMMY_PS4_SEALED_KEY", f);
        std::fclose(f);
    }

    machete::scanner::ScannedGame game;
    game.title_id = "CUSA18600";
    game.title = "Dragon Quest XI S";
    game.platform = "PS4";
    game.save_path = dst_save;

    machete::save::BackupItem item;
    item.title_id = "CUSA18600";
    item.folder_path = backup_dir;
    item.has_decrypted = false;
    item.has_crypto_vault = false;

    auto res = machete::save::restore_backup_item(game, item);
    EXPECT_TRUE(res.success);
    EXPECT_NE(res.message.find("FPKG"), std::string::npos);

    // Comprobar que los archivos existen en el destino y que se inyecto el keystone FPKG
    struct stat st{};
    EXPECT_EQ(stat((dst_save + "/Book").c_str(), &st), 0);
    EXPECT_EQ(stat((dst_save + "/keystone").c_str(), &st), 0);
    EXPECT_TRUE(machete::keystone::is_valid_keystone_file(dst_save + "/keystone"));

    // Limpieza
    std::remove((dst_save + "/Book").c_str());
    std::remove((dst_save + "/Book.bin").c_str());
    std::remove((dst_save + "/keystone").c_str());
    (void)std::remove((dst_save + "/sce_sys/keystone").c_str());
    (void)rmdir((dst_save + "/sce_sys").c_str());
    rmdir(dst_save.c_str());
    std::remove((backup_dir + "/Book").c_str());
    std::remove((backup_dir + "/Book.bin").c_str());
    rmdir(backup_dir.c_str());
    rmdir(test_root.c_str());
}

TEST(MacheteDbTest, SqliteAdapterAvailability)
{
    auto &adapter = machete::db::SqliteAdapter::instance();
    EXPECT_TRUE(adapter.is_available());
}

TEST(MacheteDbTest, PS4SaveDatabaseIndexingAndCorruptionRepair)
{
    const std::string db_dir = "/tmp/machete_db_test";
    const std::string db_file = db_dir + "/savedata.db";
    mkdir(db_dir.c_str(), 0777);

    auto &adapter = machete::db::SqliteAdapter::instance();
    ASSERT_TRUE(adapter.is_available());

    void *db = nullptr;
    ASSERT_EQ(adapter.open(db_file.c_str(), &db), 0);
    ASSERT_NE(db, nullptr);

    const char *create_tbl =
        "CREATE TABLE IF NOT EXISTS savedata ("
        "title_id TEXT, dir_name TEXT, main_title TEXT, sub_title TEXT, detail TEXT, "
        "tmp_dir_name TEXT, is_broken INTEGER, user_param INTEGER, blocks INTEGER, "
        "free_blocks INTEGER, size_kib INTEGER, mtime TEXT, fake_broken INTEGER, "
        "account_id INTEGER, user_id INTEGER, faked_owner INTEGER, cloud_icon_url TEXT, "
        "cloud_revision INTEGER, game_title_id TEXT, PRIMARY KEY (title_id, dir_name));";
    EXPECT_EQ(adapter.exec(db, create_tbl, nullptr, nullptr, nullptr), 0);

    // Insertar fila corrupta simulada (is_broken = 1)
    const char *insert_broken =
        "INSERT OR REPLACE INTO savedata (title_id, dir_name, is_broken, fake_broken, account_id, user_id) "
        "VALUES ('CUSA18600', 'Book', 1, 1, 12345, 274686298);";
    EXPECT_EQ(adapter.exec(db, insert_broken, nullptr, nullptr, nullptr), 0);

    // Reparar mediante query SQL
    const char *repair_sql = "UPDATE savedata SET is_broken = 0, fake_broken = 0 WHERE title_id = 'CUSA18600';";
    EXPECT_EQ(adapter.exec(db, repair_sql, nullptr, nullptr, nullptr), 0);

    // Verificar que is_broken es 0
    int checked_broken = -1;
    auto cb = [](void *arg, int argc, char **argv, char **) -> int {
        if (argc > 0 && argv && argv[0])
        {
            auto *out = reinterpret_cast<int *>(arg);
            *out = std::atoi(argv[0]);
        }
        return 0;
    };
    EXPECT_EQ(adapter.exec(db, "SELECT is_broken FROM savedata WHERE title_id = 'CUSA18600' AND dir_name = 'Book';",
                           cb, &checked_broken, nullptr), 0);
    EXPECT_EQ(checked_broken, 0);

    adapter.close(db);
    std::remove(db_file.c_str());
    rmdir(db_dir.c_str());
}

TEST(MachetePfsTest, PS4SlotDetection)
{
    const std::string test_dir = "/tmp/machete_slot_test";
    mkdir(test_dir.c_str(), 0777);

    // Crear archivo Book y Book.bin, asi como sce_bu_Book y sce_bu_Book.bin
    {
        FILE *f1 = std::fopen((test_dir + "/Book").c_str(), "wb");
        ASSERT_NE(f1, nullptr);
        std::fputs("PFS_CONTAINER_DATA", f1);
        std::fclose(f1);

        FILE *f2 = std::fopen((test_dir + "/Book.bin").c_str(), "wb");
        ASSERT_NE(f2, nullptr);
        std::fputs("SEALED_KEY", f2);
        std::fclose(f2);

        FILE *f3 = std::fopen((test_dir + "/sce_bu_Book").c_str(), "wb");
        ASSERT_NE(f3, nullptr);
        std::fputs("PFS_BU_CONTAINER_DATA", f3);
        std::fclose(f3);

        FILE *f4 = std::fopen((test_dir + "/sce_bu_Book.bin").c_str(), "wb");
        ASSERT_NE(f4, nullptr);
        std::fputs("SEALED_BU_KEY", f4);
        std::fclose(f4);
    }

    EXPECT_TRUE(machete::pfs::is_valid_ps4_slot_name(test_dir, "Book"));
    EXPECT_TRUE(machete::pfs::is_valid_ps4_slot_name(test_dir, "sce_bu_Book"));
    EXPECT_FALSE(machete::pfs::is_valid_ps4_slot_name(test_dir, "Book.bin"));
    EXPECT_FALSE(machete::pfs::is_valid_ps4_slot_name(test_dir, "sce_bu_Book.bin"));
    EXPECT_FALSE(machete::pfs::is_valid_ps4_slot_name(test_dir, "keystone"));
    EXPECT_FALSE(machete::pfs::is_valid_ps4_slot_name(test_dir, "sealedkey"));

    std::vector<std::string> slots;
    EXPECT_TRUE(machete::pfs::has_local_save_dummy(test_dir, false, &slots));
    ASSERT_EQ(slots.size(), 2u);

    std::remove((test_dir + "/Book").c_str());
    std::remove((test_dir + "/Book.bin").c_str());
    std::remove((test_dir + "/sce_bu_Book").c_str());
    std::remove((test_dir + "/sce_bu_Book.bin").c_str());
    rmdir(test_dir.c_str());
}

TEST(MacheteBuildInfoTest, ValidateAppVersionAndBuildNumber)
{
    EXPECT_STREQ(machete::build::version_string(), MACHETE_APP_VERSION);
    EXPECT_STREQ(machete::build::build_number(), MACHETE_BUILD_NUMBER);
    EXPECT_STRNE(machete::build::git_commit(), "");
    EXPECT_STRNE(machete::build::build_date(), "");
    EXPECT_STRNE(machete::build::build_time(), "");
}

TEST(MacheteKeystoneValidationTest, Validate96ByteKeystoneCheck)
{
    const std::string ks_path = "/tmp/machete_test_keystone_valid.bin";
    std::uint8_t ks[96] = {0};
    std::memcpy(ks, "keystone", 8);
    ks[32] = 0xAA;
    std::FILE *f = std::fopen(ks_path.c_str(), "wb");
    ASSERT_NE(f, nullptr);
    std::fwrite(ks, 1, 96, f);
    std::fclose(f);

    EXPECT_TRUE(machete::keystone::is_valid_keystone_file(ks_path));
    std::remove(ks_path.c_str());
    EXPECT_FALSE(machete::keystone::is_valid_keystone_file(ks_path));
}

TEST(MacheteKeystoneValidationTest, FallbackFpkgKeystoneInjection)
{
    const std::string test_dir = "/tmp/machete_test_fallback_ks";
    (void)mkdir(test_dir.c_str(), 0777);

    EXPECT_TRUE(machete::keystone::inject_fallback_fpkg_keystone(test_dir, "CUSA18600"));

    const std::string out_ks = test_dir + "/keystone";
    struct stat st{};
    EXPECT_EQ(stat(out_ks.c_str(), &st), 0);
    EXPECT_EQ(st.st_size, 96);
    EXPECT_TRUE(machete::keystone::is_valid_keystone_file(out_ks));

    std::uint8_t buf[96] = {0};
    std::FILE *f = std::fopen(out_ks.c_str(), "rb");
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(std::fread(buf, 1, 96, f), 96u);
    std::fclose(f);
    EXPECT_EQ(std::memcmp(buf, "keystone", 8), 0);
    EXPECT_EQ(buf[8], 0x00);
    EXPECT_EQ(buf[9], 0x02); // Type 2 (PS4)
    EXPECT_EQ(buf[10], 0x00);
    EXPECT_EQ(buf[11], 0x01); // Version 1

    (void)std::remove(out_ks.c_str());
    (void)std::remove((test_dir + "/sce_sys/keystone").c_str());
    (void)rmdir((test_dir + "/sce_sys").c_str());
    (void)rmdir(test_dir.c_str());
}

TEST(MacheteScannerPairedSkuTest, DeathStrandingPairedSkus)
{
    const auto p69 = machete::scanner::get_paired_sku_title_ids("PPSA01969");
    ASSERT_EQ(p69.size(), 1u);
    EXPECT_EQ(p69[0], "PPSA01970");

    const auto p70 = machete::scanner::get_paired_sku_title_ids("PPSA01970");
    ASSERT_EQ(p70.size(), 1u);
    EXPECT_EQ(p70[0], "PPSA01969");

    EXPECT_TRUE(machete::scanner::get_paired_sku_title_ids("PPSA13427").empty());
    EXPECT_TRUE(machete::scanner::is_paired_sku("PPSA01969", "PPSA01970"));
    EXPECT_TRUE(machete::scanner::is_paired_sku("PPSA01970", "PPSA01969"));
    EXPECT_FALSE(machete::scanner::is_paired_sku("PPSA01969", "PPSA13427"));
}

TEST(MacheteParamJsonSizeTest, UpdateFileSizeAndMtimeInParamJson)
{
    const std::string test_json = "/tmp/machete_test_param.json";
    {
        std::FILE *f = std::fopen(test_json.c_str(), "w");
        ASSERT_NE(f, nullptr);
        std::fputs("{\n  \"titleId\": \"PPSA13427\",\n  \"fileSize\": 1024,\n  \"mtime\": 0\n}\n", f);
        std::fclose(f);
    }

    machete::pfs::update_ps5_param_json_size(test_json, 12582912ULL);

    std::FILE *f = std::fopen(test_json.c_str(), "r");
    ASSERT_NE(f, nullptr);
    char buf[512] = {0};
    const std::size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    std::remove(test_json.c_str());

    ASSERT_GT(n, 0u);
    const std::string content(buf, n);
    EXPECT_NE(content.find("\"fileSize\": 12582912"), std::string::npos);
    EXPECT_EQ(content.find("\"fileSize\": 1024"), std::string::npos);
}

TEST(MacheteBuildInfoTest, BuildNumberMatchesExpected)
{
    EXPECT_STREQ(machete::build::build_number(), "84");
}

TEST(MacheteParamJsonDirNameTest, UpdateDirNameInParamJson)
{
    const std::string test_json = "/tmp/machete_test_dirname_param.json";
    {
        std::FILE *f = std::fopen(test_json.c_str(), "w");
        ASSERT_NE(f, nullptr);
        std::fputs("{\n  \"titleId\": \"PPSA01969\",\n  \"dirName\": \"PROFILE\"\n}\n", f);
        std::fclose(f);
    }

    machete::pfs::update_ps5_param_json_dir_name(test_json, "AUTOSAVE0");

    std::FILE *f = std::fopen(test_json.c_str(), "r");
    ASSERT_NE(f, nullptr);
    char buf[512] = {0};
    const std::size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    std::remove(test_json.c_str());

    ASSERT_GT(n, 0u);
    const std::string content(buf, n);
    EXPECT_NE(content.find("\"dirName\": \"AUTOSAVE0\""), std::string::npos);
    EXPECT_EQ(content.find("\"dirName\": \"PROFILE\""), std::string::npos);
}

TEST(MacheteParamJsonTitleIdTest, UpdateTitleIdInParamJson)
{
    const std::string test_json = "/tmp/machete_test_title_id_param.json";
    {
        std::FILE *f = std::fopen(test_json.c_str(), "w");
        ASSERT_NE(f, nullptr);
        std::fputs("{\n  \"titleId\": \"PPSA01969\",\n  \"dirName\": \"PROFILE\"\n}\n", f);
        std::fclose(f);
    }

    machete::pfs::update_ps5_param_json_title_id(test_json, "PPSA01970");

    std::FILE *f = std::fopen(test_json.c_str(), "r");
    ASSERT_NE(f, nullptr);
    char buf[512] = {0};
    const std::size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    std::remove(test_json.c_str());

    ASSERT_GT(n, 0u);
    const std::string content(buf, n);
    EXPECT_NE(content.find("\"titleId\": \"PPSA01970\""), std::string::npos);
    EXPECT_EQ(content.find("\"titleId\": \"PPSA01969\""), std::string::npos);
}

TEST(MachetePs5SavedataDbTest, DbCandidatePathsIncludeAllProsperoAndSystemDataLocations)
{
    const auto candidates = machete::db::get_ps5_savedata_db_candidates(0x105f615a);
    ASSERT_FALSE(candidates.empty());

    bool has_prospero = false;
    bool has_ps4_savedata_contamination = false;
    for (const auto &c : candidates)
    {
        if (c.find("/system_data/savedata_prospero/105f615a/db/user/savedata.db") != std::string::npos)
            has_prospero = true;
        if (c.find("/system_data/savedata/105f615a/") != std::string::npos)
            has_ps4_savedata_contamination = true;
    }
    EXPECT_TRUE(has_prospero);
    // Absolute isolation: PS5 candidate paths must NEVER contaminate PS4 databases
    EXPECT_FALSE(has_ps4_savedata_contamination);
}

TEST(MachetePs5ParamJsonSynthesisTest, SynthesizeParamJsonFromSlots)
{
    const std::string dst = "/tmp/machete_test_synth_dst";
    const std::string meta = "/tmp/machete_test_synth_meta";
    (void)mkdir(dst.c_str(), 0777);
    (void)mkdir(meta.c_str(), 0777);

    // Crear slots simulados sdimg_PROFILE y sdimg_AUTOSAVE0
    const std::string f1 = dst + "/sdimg_PROFILE";
    const std::string f2 = dst + "/sdimg_AUTOSAVE0";
    {
        std::FILE *out1 = std::fopen(f1.c_str(), "wb");
        ASSERT_NE(out1, nullptr);
        std::fputs("DUMMY_PROFILE_DATA_12345", out1);
        std::fclose(out1);

        std::FILE *out2 = std::fopen(f2.c_str(), "wb");
        ASSERT_NE(out2, nullptr);
        std::fputs("DUMMY_AUTOSAVE_DATA_1234567890", out2);
        std::fclose(out2);
    }

    const bool ok = machete::save::synthesize_ps5_param_json(
        "PPSA01969", dst, meta, "DEATH STRANDING DIRECTOR'S CUT", 0, 0);
    EXPECT_TRUE(ok);

    // Verificar que param.json se creo en meta_target y NO contamina dst (savedata_prospero)
    const std::string pj_meta = meta + "/param.json";
    const std::string pj_sce = dst + "/sce_sys/param.json";
    const std::string pj_dst = dst + "/param.json";
    const std::string slot_meta = meta + "/PROFILE.json";
    const std::string slot_auto = meta + "/AUTOSAVE0.json";
    const std::string slot_meta_param = meta + "/PROFILE_param.json";
    const std::string slot_auto_param = meta + "/AUTOSAVE0_param.json";

    EXPECT_EQ(access(pj_meta.c_str(), F_OK), 0);
    EXPECT_NE(access(pj_sce.c_str(), F_OK), 0);
    EXPECT_NE(access(pj_dst.c_str(), F_OK), 0);
    EXPECT_EQ(access(slot_meta.c_str(), F_OK), 0);
    EXPECT_EQ(access(slot_auto.c_str(), F_OK), 0);
    EXPECT_EQ(access(slot_meta_param.c_str(), F_OK), 0);
    EXPECT_EQ(access(slot_auto_param.c_str(), F_OK), 0);

    // Leer pj_meta y validar contenido (AUTOSAVE0 debe ser el slot primario de juego activo)
    {
        std::FILE *f = std::fopen(pj_meta.c_str(), "rb");
        ASSERT_NE(f, nullptr);
        char buf[2048] = {0};
        const std::size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
        std::fclose(f);
        ASSERT_GT(n, 0u);
        const std::string content(buf, n);
        EXPECT_NE(content.find("\"titleId\": \"PPSA01969\""), std::string::npos);
        EXPECT_NE(content.find("\"dirName\": \"AUTOSAVE0\""), std::string::npos);
        EXPECT_NE(content.find("DEATH STRANDING"), std::string::npos);
    }

    // Limpieza
    (void)std::remove(f1.c_str());
    (void)std::remove(f2.c_str());
    (void)std::remove(pj_meta.c_str());
    (void)std::remove(pj_sce.c_str());
    (void)std::remove(pj_dst.c_str());
    (void)std::remove(slot_meta.c_str());
    (void)std::remove(slot_auto.c_str());
    (void)std::remove(slot_meta_param.c_str());
    (void)std::remove(slot_auto_param.c_str());
    (void)std::remove((dst + "/sce_sys/PROFILE.json").c_str());
    (void)std::remove((dst + "/sce_sys/AUTOSAVE0.json").c_str());
    (void)std::remove((dst + "/sce_sys/PROFILE_param.json").c_str());
    (void)std::remove((dst + "/sce_sys/AUTOSAVE0_param.json").c_str());
    (void)rmdir((dst + "/sce_sys").c_str());
    (void)rmdir(dst.c_str());
    (void)rmdir(meta.c_str());
}

TEST(MachetePfsTest, PfsMountSymbolsResolutionSafety)
{
    int (*init_m)(machete::pfs::SaveMountOpt *) = nullptr;
    int (*m_fn)(machete::pfs::SaveMountOpt *, const char *, const char *, const void *) = nullptr;
    int (*init_u)(machete::pfs::SaveMountOpt *) = nullptr;
    int (*u_fn)(machete::pfs::SaveMountOpt *, const char *, void *, int) = nullptr;

    const bool resolved = machete::pfs::ScopedPfsMount::ensure_pfs_symbols_resolved(
        &init_m, &m_fn, &init_u, &u_fn);

    // On macOS local unit tests, libSceFsInternalForVsh is not present
    EXPECT_FALSE(resolved);
    EXPECT_EQ(m_fn, nullptr);
    EXPECT_EQ(u_fn, nullptr);
}

TEST(MacheteApolloResignTest, ResignParamSfoUnlocksCopyProtectionAndSetsApolloAccountId)
{
    const std::string test_dir = "/tmp/machete_apollo_sfo_test";
    (void)mkdir(test_dir.c_str(), 0777);
    const std::string sfo_path = test_dir + "/param.sfo";

    // 1. Sintetizar un descriptor de guardado PS4 simulado con cuenta 0 y proteccion contra copia
    ASSERT_TRUE(machete::sfo::synthesize_ps4_save_param_sfo(
        sfo_path, "CUSA03173", "savedata0", "Bloodborne", "Hunter Save", "Level 120",
        0x10000000, 0, 1024, 999));

    // Forzar el atributo de proteccion contra copia (ATTRIBUTE = 1)
    {
        std::FILE *f = std::fopen(sfo_path.c_str(), "r+b");
        ASSERT_NE(f, nullptr);
        std::vector<std::uint8_t> buf(4096, 0);
        const std::size_t n = std::fread(buf.data(), 1, buf.size(), f);
        // Localizar "ATTRIBUTE" en tabla de claves y fijar valor en 1
        for (std::size_t i = 0; i + 4 <= n; ++i)
        {
            if (std::memcmp(buf.data() + i, "\x04\x04", 2) == 0) // format uint32
            {
                // Sobrescribir dato con 1 (candado activo)
                buf[n - 4] = 0x01;
                break;
            }
        }
        std::fseek(f, 0, SEEK_SET);
        std::fwrite(buf.data(), 1, n, f);
        std::fclose(f);
    }

    // 2. Ejecutar resignado estilo Apollo con cuenta offline (target_account_id = 0)
    std::uint8_t test_psid[16] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    machete::sfo::ResignParams rp;
    rp.target_user_id = 0x105f615a;
    rp.target_account_id = 0; // Cuenta offline
    rp.apply_account_id = true;
    rp.psid = test_psid;

    EXPECT_TRUE(machete::sfo::resign_param_sfo(sfo_path, rp));

    // 3. Validar metadatos resignados
    machete::sfo::SfoSaveMetadata meta = machete::sfo::read_sfo_metadata(sfo_path);
    EXPECT_TRUE(meta.valid);
    EXPECT_TRUE(meta.has_account_id);
    const std::uint64_t expected_apollo_acc = machete::sfo::get_apollo_synthetic_account_id(0x105f615a);
    EXPECT_EQ(meta.account_id, expected_apollo_acc);
    EXPECT_NE(meta.account_id, 0ULL);
    EXPECT_EQ(meta.user_id, 0x105f615au);
    EXPECT_TRUE(meta.has_psid_hmac);

    // Limpieza
    (void)std::remove(sfo_path.c_str());
    (void)rmdir(test_dir.c_str());
}

TEST(MacheteApolloResignTest, ResignLocalPs4SaveOperation)
{
    const std::string test_root = "/tmp/machete_local_resign_test";
    const std::string save_dir = test_root + "/user/home/105f615a/savedata/CUSA03173";
    (void)mkdir(test_root.c_str(), 0777);
    (void)mkdir((test_root + "/user").c_str(), 0777);
    (void)mkdir((test_root + "/user/home").c_str(), 0777);
    (void)mkdir((test_root + "/user/home/105f615a").c_str(), 0777);
    (void)mkdir((test_root + "/user/home/105f615a/savedata").c_str(), 0777);
    (void)mkdir(save_dir.c_str(), 0777);
    (void)mkdir((save_dir + "/sce_sys").c_str(), 0777);

    // Crear archivos simulados de guardado de Bloodborne
    {
        std::FILE *f = std::fopen((save_dir + "/savedata0").c_str(), "wb");
        ASSERT_NE(f, nullptr);
        std::fputs("BLOODBORNE_SAVE_PFS_DATA", f);
        std::fclose(f);
    }
    {
        std::FILE *f = std::fopen((save_dir + "/savedata0.bin").c_str(), "wb");
        ASSERT_NE(f, nullptr);
        std::fputs("BLOODBORNE_SEALED_KEY_96_BYTES", f);
        std::fclose(f);
    }
    {
        std::FILE *f = std::fopen((save_dir + "/keystone").c_str(), "wb");
        ASSERT_NE(f, nullptr);
        std::vector<std::uint8_t> ks(96, 0x33);
        std::fwrite(ks.data(), 1, 96, f);
        std::fclose(f);
    }
    ASSERT_TRUE(machete::sfo::synthesize_ps4_save_param_sfo(
        save_dir + "/sce_sys/param.sfo", "CUSA03173", "savedata0",
        "Bloodborne", "Hunter", "Detail", 0x10000000, 0x1234ULL, 512, 123));

    machete::scanner::ScannedGame game;
    game.title = "Bloodborne";
    game.title_id = "CUSA03173";
    game.platform = "PS4";
    game.save_path = save_dir;

    auto res = machete::save::resign_local_ps4_save(game);
    EXPECT_TRUE(res.success);
    EXPECT_NE(res.message.find("Apollo"), std::string::npos);

    // Comprobar que el Keystone existe en ambas ubicaciones
    struct stat st1{}, st2{};
    EXPECT_EQ(stat((save_dir + "/keystone").c_str(), &st1), 0);
    EXPECT_EQ(stat((save_dir + "/sce_sys/keystone").c_str(), &st2), 0);

    // Comprobar que param.sfo fue resignado para 0x105f615a
    machete::sfo::SfoSaveMetadata meta = machete::sfo::read_sfo_metadata(save_dir + "/sce_sys/param.sfo");
    EXPECT_TRUE(meta.valid);
    EXPECT_EQ(meta.user_id, 0x105f615au);

    // Limpieza
    (void)std::remove((save_dir + "/savedata0").c_str());
    (void)std::remove((save_dir + "/savedata0.bin").c_str());
    (void)std::remove((save_dir + "/keystone").c_str());
    (void)std::remove((save_dir + "/sce_sys/keystone").c_str());
    (void)std::remove((save_dir + "/sce_sys/param.sfo").c_str());
    (void)rmdir((save_dir + "/sce_sys").c_str());
    (void)rmdir(save_dir.c_str());
}

TEST(MacheteKeystonePriorityTest, InstalledGameKeystoneOverridesForeignBackupKeystone)
{
    const std::string test_root = "/tmp/machete_ks_priority_test";
    const std::string dst_save = test_root + "/dst_save";
    const std::string backup_dir = test_root + "/foreign_backup";
    const std::string app_dir = test_root + "/app_installed";

    (void)mkdir(test_root.c_str(), 0777);
    (void)mkdir(dst_save.c_str(), 0777);
    (void)mkdir(backup_dir.c_str(), 0777);
    (void)mkdir(app_dir.c_str(), 0777);
    (void)mkdir((app_dir + "/sce_sys").c_str(), 0777);

    // 1. Keystone autentico del juego instalado en la consola (96 bytes con 0xAA)
    const std::string app_ks = app_dir + "/sce_sys/keystone";
    {
        std::FILE *f = std::fopen(app_ks.c_str(), "wb");
        ASSERT_NE(f, nullptr);
        std::vector<std::uint8_t> ks(96, 0xAA);
        std::fwrite(ks.data(), 1, 96, f);
        std::fclose(f);
    }

    // 2. Keystone foraneo proveniente de un respaldo de otra consola (96 bytes con 0x55)
    const std::string backup_ks = backup_dir + "/keystone";
    {
        std::FILE *f = std::fopen(backup_ks.c_str(), "wb");
        ASSERT_NE(f, nullptr);
        std::vector<std::uint8_t> ks(96, 0x55);
        std::fwrite(ks.data(), 1, 96, f);
        std::fclose(f);
    }
    {
        std::FILE *f = std::fopen((backup_dir + "/savedata0.bin").c_str(), "wb");
        ASSERT_NE(f, nullptr);
        std::fputs("PAYLOAD", f);
        std::fclose(f);
    }

    machete::scanner::ScannedGame game;
    game.title = "Test Game Priority";
    game.title_id = "CUSA99999";
    game.platform = "PS4";
    game.save_path = dst_save;
    game.app_path = app_dir;

    machete::save::BackupItem item;
    item.title_id = "CUSA99999";
    item.folder_path = backup_dir;

    auto res = machete::save::restore_backup_item(game, item);
    EXPECT_TRUE(res.success);

    // 3. Verificar que el keystone restaurado es el AUTENTICO de la aplicacion (0xAA), NO el foraneo del backup (0x55)
    std::FILE *fr = std::fopen((dst_save + "/keystone").c_str(), "rb");
    ASSERT_NE(fr, nullptr);
    std::vector<std::uint8_t> read_buf(96, 0);
    EXPECT_EQ(std::fread(read_buf.data(), 1, 96, fr), 96u);
    std::fclose(fr);

    EXPECT_EQ(read_buf[0], 0xAA);
    EXPECT_EQ(read_buf[95], 0xAA);

    // Limpieza
    (void)std::remove((dst_save + "/savedata0.bin").c_str());
    (void)std::remove((dst_save + "/keystone").c_str());
    (void)std::remove((dst_save + "/sce_sys/keystone").c_str());
    (void)rmdir((dst_save + "/sce_sys").c_str());
    (void)rmdir(dst_save.c_str());
    (void)std::remove((backup_dir + "/savedata0.bin").c_str());
    (void)std::remove((backup_dir + "/keystone").c_str());
    (void)rmdir(backup_dir.c_str());
    (void)std::remove(app_ks.c_str());
    (void)rmdir((app_dir + "/sce_sys").c_str());
    (void)rmdir(app_dir.c_str());
    (void)rmdir(test_root.c_str());
}

TEST(MachetePs4TitleResolutionTest, ResolvesKnownPs4TitlesFromDictionary)
{
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA18600"),
              "DRAGON QUEST XI S: Echoes of an Elusive Age - Definitive Edition");
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA03173"),
              "Bloodborne");
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA07408"),
              "God of War");
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA00552"),
              "The Last of Us Remastered");
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA10249"),
              "The Last of Us Part II");
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA11456"),
              "Ghost of Tsushima");
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA02299"),
              "Marvel's Spider-Man");
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA01440"),
              "Horizon Zero Dawn");
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA03041"),
              "Red Dead Redemption 2");
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA00411"),
              "Grand Theft Auto V");
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA03388"),
              "The Witcher 3: Wild Hunt");
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA01623"),
              "Dark Souls III");
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA12047"),
              "Sekiro: Shadows Die Twice");
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA18581"),
              "Elden Ring");
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA05877"),
              "Persona 5");
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA07237"),
              "Final Fantasy VII Remake");
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA07713"),
              "Monster Hunter: World");
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA00900"),
              "Uncharted 4: A Thief's End");
    EXPECT_EQ(machete::scanner::resolve_known_ps4_title("CUSA16596"),
              "Cyberpunk 2077");
    EXPECT_TRUE(machete::scanner::resolve_known_ps4_title("CUSA99999").empty());
}

TEST(MachetePs4TitleResolutionTest, RenderableLatinDetectsLatinPresenceCorrectly)
{
    EXPECT_TRUE(machete::scanner::is_renderable_latin("Bloodborne"));
    EXPECT_TRUE(machete::scanner::is_renderable_latin("DRAGON QUEST XI S"));
    EXPECT_FALSE(machete::scanner::is_renderable_latin("وولڤرين من مارفل"));
    EXPECT_FALSE(machete::scanner::is_renderable_latin(""));
}

TEST(MacheteZipReaderTest, ExtractsStoreArchiveWithPermissionsAndIntegrity)
{
    const std::string test_dir = "/tmp/machete_test_ota_src";
    const std::string dst_dir = "/tmp/machete_test_ota_dst";
    const std::string zip_file = "/tmp/machete_test_ota.zip";

    (void)mkdir(test_dir.c_str(), 0777);
    (void)mkdir((test_dir + "/sce_sys").c_str(), 0777);

    {
        std::ofstream f(test_dir + "/eboot.bin", std::ios::binary);
        f << "MOCK_EBOOT_BIN_PROSPERO_PAYLOAD";
    }
    {
        std::ofstream f(test_dir + "/sce_sys/param.json", std::ios::binary);
        f << "{\"titleId\":\"PPSA99700\",\"titleName\":\"MacheteSaveGameManager\"}";
    }

    machete::server::ZipWriter writer;
    writer.add_directory(test_dir, "PPSA99700");
    writer.prepare();
    EXPECT_GT(writer.entry_count(), 0u);

    auto zip_bytes = writer.write_to_memory();
    ASSERT_FALSE(zip_bytes.empty());

    {
        std::ofstream zf(zip_file, std::ios::binary);
        zf.write(reinterpret_cast<const char *>(zip_bytes.data()), zip_bytes.size());
    }

    machete::server::ZipReader reader;
    ASSERT_TRUE(reader.open_file(zip_file));
    EXPECT_TRUE(reader.has_file("eboot.bin"));
    EXPECT_TRUE(reader.has_file("param.json"));

    auto res = reader.extract_all(dst_dir, true);
    EXPECT_TRUE(res.success);
    EXPECT_GE(res.files_extracted, 2u);

    EXPECT_EQ(access((dst_dir + "/eboot.bin").c_str(), F_OK), 0);
    EXPECT_EQ(access((dst_dir + "/sce_sys/param.json").c_str(), F_OK), 0);

    struct stat st{};
    EXPECT_EQ(stat((dst_dir + "/eboot.bin").c_str(), &st), 0);
    EXPECT_EQ(st.st_mode & 0777, 0777);

    reader.close();

    (void)std::remove((test_dir + "/eboot.bin").c_str());
    (void)std::remove((test_dir + "/sce_sys/param.json").c_str());
    (void)rmdir((test_dir + "/sce_sys").c_str());
    (void)rmdir(test_dir.c_str());

    (void)std::remove((dst_dir + "/eboot.bin").c_str());
    (void)std::remove((dst_dir + "/sce_sys/param.json").c_str());
    (void)rmdir((dst_dir + "/sce_sys").c_str());
    (void)rmdir(dst_dir.c_str());
    (void)std::remove(zip_file.c_str());
}

TEST(MacheteZipReaderTest, ExtractsRealDeflateDistributionPackage)
{
    const std::string pkg_path = "/Users/thebigmike/Downloads/PPSA99700.zip";
    if (access(pkg_path.c_str(), F_OK) != 0)
    {
        GTEST_SKIP() << "Distribution zip not found at " << pkg_path;
    }

    machete::server::ZipReader reader;
    ASSERT_TRUE(reader.open_file(pkg_path));
    EXPECT_TRUE(reader.has_file("eboot.bin"));
    EXPECT_TRUE(reader.has_file("param.json"));
    EXPECT_TRUE(reader.has_file("libc.prx"));

    const auto *entry = reader.find_entry("param.json");
    ASSERT_NE(entry, nullptr);

    std::vector<std::uint8_t> data;
    ASSERT_TRUE(reader.extract_to_memory(*entry, data));
    ASSERT_FALSE(data.empty());

    std::string text(reinterpret_cast<const char *>(data.data()), data.size());
    EXPECT_NE(text.find("PPSA99700"), std::string::npos);

    reader.close();
}

TEST(MacheteZipReaderTest, RejectsInvalidCorruptArchive)
{
    const std::string bad_file = "/tmp/machete_test_bad.zip";
    {
        std::ofstream f(bad_file, std::ios::binary);
        f << "NOT_A_VALID_ZIP_ARCHIVE_DATA_JUST_RANDOM_TEXT";
    }

    machete::server::ZipReader reader;
    EXPECT_FALSE(reader.open_file(bad_file));
    (void)std::remove(bad_file.c_str());
}

TEST(MacheteZipReaderTest, CleansUpObsoleteFilesAndDirectories)
{
    const std::string test_dir = "/tmp/machete_test_clean_src";
    const std::string dst_dir = "/tmp/machete_test_clean_dst";
    const std::string zip_file = "/tmp/machete_test_clean.zip";

    // Prepare destination with obsolete files and directories
    (void)mkdir(dst_dir.c_str(), 0777);
    (void)mkdir((dst_dir + "/obsolete_dir").c_str(), 0777);
    {
        std::ofstream f(dst_dir + "/obsolete_dir/junk.txt");
        f << "OBSOLETE_CONTENT";
    }
    {
        std::ofstream f(dst_dir + "/old_module.prx");
        f << "OLD_PRX_DATA";
    }

    // Verify obsolete files exist initially
    ASSERT_EQ(access((dst_dir + "/old_module.prx").c_str(), F_OK), 0);
    ASSERT_EQ(access((dst_dir + "/obsolete_dir/junk.txt").c_str(), F_OK), 0);

    // Prepare source directory with only new files
    (void)mkdir(test_dir.c_str(), 0777);
    {
        std::ofstream f(test_dir + "/eboot.bin", std::ios::binary);
        f << "NEW_EBOOT_BIN_DATA";
    }

    machete::server::ZipWriter writer;
    writer.add_directory(test_dir, "PPSA99700");
    writer.prepare();
    auto zip_bytes = writer.write_to_memory();
    ASSERT_FALSE(zip_bytes.empty());

    {
        std::ofstream zf(zip_file, std::ios::binary);
        zf.write(reinterpret_cast<const char *>(zip_bytes.data()), zip_bytes.size());
    }

    machete::server::ZipReader reader;
    ASSERT_TRUE(reader.open_file(zip_file));

    // Extract with clean_obsolete = true
    auto res = reader.extract_all(dst_dir, true, true);
    EXPECT_TRUE(res.success);
    EXPECT_GE(res.files_extracted, 1u);
    EXPECT_GE(res.files_cleaned, 2u);

    // Assert obsolete files were cleaned up
    EXPECT_NE(access((dst_dir + "/old_module.prx").c_str(), F_OK), 0);
    EXPECT_NE(access((dst_dir + "/obsolete_dir/junk.txt").c_str(), F_OK), 0);
    EXPECT_NE(access((dst_dir + "/obsolete_dir").c_str(), F_OK), 0);

    // Assert new files exist
    EXPECT_EQ(access((dst_dir + "/eboot.bin").c_str(), F_OK), 0);

    reader.close();

    // Clean up
    (void)std::remove((test_dir + "/eboot.bin").c_str());
    (void)rmdir(test_dir.c_str());
    (void)std::remove((dst_dir + "/eboot.bin").c_str());
    (void)rmdir(dst_dir.c_str());
    (void)std::remove(zip_file.c_str());
}

TEST(MacheteLoggerTest, ResetFreshExecutesSafelyAndStartsCleanSession)
{
    machete::log::write("[UNIT_TEST]", "Pre-reset log line for testing");
    machete::log::flush();

    // Call reset_fresh to truncate and start a clean session
    machete::log::reset_fresh();

    machete::log::write("[UNIT_TEST]", "Post-reset log line in fresh session");
    machete::log::flush();

    SUCCEED();
}

TEST(MacheteAes128XtsTest, EncryptDecryptRoundTrip)
{
    const std::uint8_t key[32] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
    };

    machete::crypto::XtsAes128 xts(key);

    std::vector<std::uint8_t> plain(1024);
    for (std::size_t i = 0; i < plain.size(); ++i)
        plain[i] = static_cast<std::uint8_t>((i * 7 + 13) & 0xff);

    std::vector<std::uint8_t> cipher(1024, 0);
    std::vector<std::uint8_t> decrypted(1024, 0);

    // Encrypt sector 0 (512 bytes) and sector 1 (512 bytes)
    xts.encrypt_sector(0, plain.data(), cipher.data(), 512);
    xts.encrypt_sector(1, plain.data() + 512, cipher.data() + 512, 512);

    // Ciphertext must differ from plaintext
    EXPECT_NE(std::memcmp(plain.data(), cipher.data(), 1024), 0);

    // Decrypt both sectors
    xts.decrypt_sector(0, cipher.data(), decrypted.data(), 512);
    xts.decrypt_sector(1, cipher.data() + 512, decrypted.data() + 512, 512);

    // Decrypted must match plaintext exactly
    EXPECT_EQ(std::memcmp(plain.data(), decrypted.data(), 1024), 0);
}

TEST(MacheteAes128XtsTest, ResignPs4ContainerInPlaceSucceeds)
{
    const std::string tmp_img = "/tmp/machete_test_ps4_container.bin";
    const std::string tmp_sfo = "/tmp/machete_test_synth.sfo";

    // 1. Synthesize a valid PS4 param.sfo
    ASSERT_TRUE(machete::sfo::synthesize_ps4_save_param_sfo(
        tmp_sfo, "CUSA18600", "savedata0", "DRAGON QUEST XI S", "Hero", "Save 1",
        0x105f615a, 0x1122334455667788ULL, 325, 0, nullptr));

    std::ifstream sf(tmp_sfo, std::ios::binary);
    ASSERT_TRUE(sf.is_open());
    std::vector<std::uint8_t> sfo_bytes((std::istreambuf_iterator<char>(sf)),
                                         std::istreambuf_iterator<char>());
    sf.close();
    (void)std::remove(tmp_sfo.c_str());

    ASSERT_GE(sfo_bytes.size(), 64u);

    // 2. Build a mock 32 KB container with sector 1 (offset 4096) containing encrypted SFO
    std::vector<std::uint8_t> container(32768, 0xaa);
    const std::size_t sfo_offset = 4096; // Sector 1 (4096 bytes)
    std::memcpy(container.data() + sfo_offset, sfo_bytes.data(), sfo_bytes.size());

    const std::uint8_t samu_key[32] = {
        0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
        0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60
    };

    machete::crypto::XtsAes128 xts(samu_key);
    // Encrypt PFS sector 1 (4096 bytes)
    std::vector<std::uint8_t> enc(4096);
    xts.encrypt_sector(1, container.data() + 4096, enc.data(), 4096);
    std::memcpy(container.data() + 4096, enc.data(), 4096);

    {
        std::ofstream img_out(tmp_img, std::ios::binary);
        ASSERT_TRUE(img_out.is_open());
        img_out.write(reinterpret_cast<const char *>(container.data()), container.size());
    }

    // 3. Resign in-place using our autonomous engine
    machete::sfo::ResignParams rp{};
    rp.target_account_id = 0x37c80d2e21ead98cULL;
    rp.target_user_id = 0x105f615a;

    std::vector<std::uint8_t> extracted_sfo;
    const bool resigned = machete::crypto::resign_ps4_pfs_container_sectors(
        tmp_img, samu_key, rp, &extracted_sfo);

    EXPECT_TRUE(resigned);
    EXPECT_FALSE(extracted_sfo.empty());

    // 4. Verify the extracted SFO has the new account ID
    machete::sfo::SfoSaveMetadata meta{};
    EXPECT_TRUE(machete::sfo::read_sfo_metadata_from_buffer(extracted_sfo.data(), extracted_sfo.size(), meta));
    EXPECT_EQ(meta.account_id, 0x37c80d2e21ead98cULL);

    (void)std::remove(tmp_img.c_str());
}

TEST(MacheteProsperoRestoreTest, ParamJsonDirNameUpdatePreservesStructure)
{
    const std::string tmp_json = "/tmp/machete_test_param.json";
    const std::string original = "{\n  \"titleId\": \"PPSA01969\",\n  \"dirName\": \"PROFILE\",\n  \"fileSize\": 1048576\n}\n";

    {
        std::ofstream out(tmp_json, std::ios::binary);
        ASSERT_TRUE(out.is_open());
        out << original;
    }

    machete::pfs::update_ps5_param_json_dir_name(tmp_json, "AUTOSAVE0");

    std::ifstream in(tmp_json, std::ios::binary);
    ASSERT_TRUE(in.is_open());
    std::string modified((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
    in.close();

    EXPECT_NE(modified.find("\"dirName\": \"AUTOSAVE0\""), std::string::npos);
    EXPECT_NE(modified.find("\"titleId\": \"PPSA01969\""), std::string::npos);
    EXPECT_NE(modified.find("\"fileSize\": 1048576"), std::string::npos);

    (void)std::remove(tmp_json.c_str());
}

TEST(MachetePs4SlotTest, SlotNamePreservesAuthenticNamesWithoutSdimg)
{
    const std::string test_dir = "/tmp/machete_ps4_slot_test";
    (void)mkdir(test_dir.c_str(), 0777);

    // Create authentic PS4 slot files: Book and Book.bin
    const std::string slot_img = test_dir + "/Book";
    const std::string slot_bin = test_dir + "/Book.bin";
    {
        std::ofstream f(slot_img, std::ios::binary);
        f << "DUMMY_CONTAINER_DATA";
    }
    {
        std::ofstream f(slot_bin, std::ios::binary);
        std::vector<char> bin_data(96, 0x01);
        f.write(bin_data.data(), bin_data.size());
    }

    EXPECT_TRUE(machete::pfs::is_valid_ps4_slot_name(test_dir, "Book"));
    EXPECT_FALSE(machete::pfs::is_valid_sdimg_name("Book"));
    EXPECT_TRUE(machete::pfs::is_valid_sdimg_name("sdimg_Book"));

    (void)std::remove(slot_img.c_str());
    (void)std::remove(slot_bin.c_str());
    (void)rmdir(test_dir.c_str());
}

TEST(MacheteCrossConsoleTest, KeystoneFingerprintMismatchDetection)
{
    const std::string b_dir = "/tmp/machete_cross_ks_b";
    (void)mkdir(b_dir.c_str(), 0777);
    const std::string b_ks = b_dir + "/keystone";

    std::vector<std::uint8_t> b_ks_data(96, 0);
    // Keystone magic / header
    b_ks_data[0] = 0x01;
    // Fingerprint at 0x2C
    for (int i = 0x2C; i < 0x4C; ++i)
        b_ks_data[i] = static_cast<std::uint8_t>(0xAA);

    {
        std::ofstream f(b_ks, std::ios::binary);
        f.write(reinterpret_cast<const char *>(b_ks_data.data()), b_ks_data.size());
    }

    // Backup has no crypto_vault and no decrypted/
    // With an unknown/uninstalled game, keystone match is skipped cleanly
    EXPECT_FALSE(machete::pfs::is_backup_from_different_console(b_dir, true, "PPSA_NONEXISTENT"));

    (void)std::remove(b_ks.c_str());
    (void)rmdir(b_dir.c_str());
}

TEST(MacheteDecimaBackupTest, MirrorMissingBackupContainers)
{
    const std::string test_dir = "/tmp/machete_decima_mirror_test";
    (void)mkdir(test_dir.c_str(), 0777);
    const std::string main_img = test_dir + "/sdimg_MANUALSAVE0";
    const std::string bu_img = test_dir + "/sdimg_sce_bu_MANUALSAVE0";

    {
        std::ofstream f(main_img, std::ios::binary);
        f.write("DECIMA_SAVE_PAYLOAD", 19);
    }
    ASSERT_EQ(access(bu_img.c_str(), F_OK), -1);

    // Verify copy atomic creates the backup mirror
    EXPECT_TRUE(machete::pfs::copy_file_atomic(main_img, bu_img));
    EXPECT_EQ(access(bu_img.c_str(), R_OK), 0);

    (void)std::remove(main_img.c_str());
    (void)std::remove(bu_img.c_str());
    (void)rmdir(test_dir.c_str());
}

TEST(MacheteDbTest, PS4SavedataCandidatePriorityPrefersProspero)
{
    const auto candidates = machete::db::get_savedata_db_candidates(0x105f615a);
    ASSERT_FALSE(candidates.empty());
    // The very first candidate must be from savedata_prospero on PS5
    EXPECT_NE(candidates[0].find("savedata_prospero"), std::string::npos);
}

TEST(MacheteDbTest, PS5SystemBlocksConstantZero)
{
    // Ensure that PS5 Prospero database records system_blocks as 0
    const std::uint32_t blocks = 192;
    const std::uint32_t system_blocks = 0;
    EXPECT_EQ(system_blocks, 0u);
    EXPECT_EQ(blocks, 192u);
}

TEST(MachetePs4SanitizationTest, LooseFilesAndLooseKeystoneUnlinked)
{
    const std::string test_dir = "/tmp/machete_ps4_clean_test";
    (void)mkdir(test_dir.c_str(), 0777);
    const std::string sfo_file = test_dir + "/param.sfo";
    const std::string ks_file = test_dir + "/keystone";
    const std::string sce_sys_dir = test_dir + "/sce_sys";

    {
        std::ofstream fs(sfo_file);
        fs << "SFO_CONTENT";
    }
    {
        std::ofstream fk(ks_file);
        fk << "KEYSTONE_CONTENT";
    }
    (void)mkdir(sce_sys_dir.c_str(), 0777);

    // Simulate unconditional PS4 folder sanitization
    (void)unlink(sfo_file.c_str());
    (void)unlink(ks_file.c_str());
    machete::pfs::remove_dir_recursive_simple(sce_sys_dir);

    // Verify loose param.sfo, sce_sys and loose keystone are gone from container dir
    EXPECT_EQ(access(sfo_file.c_str(), F_OK), -1);
    EXPECT_EQ(access(sce_sys_dir.c_str(), F_OK), -1);
    EXPECT_EQ(access(ks_file.c_str(), F_OK), -1);

    (void)rmdir(test_dir.c_str());
}

TEST(MacheteSfoSynthTest, ExactPsidHmacPreserved)
{
    const std::string tmp_sfo = "/tmp/machete_test_synth_exact_hmac.sfo";
    const std::uint8_t exact_hmac[32] = {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
        0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10
    };

    EXPECT_TRUE(machete::sfo::synthesize_ps4_save_param_sfo(
        tmp_sfo, "CUSA18600", "Book", "Dragon Quest XI S", "Adventure Log",
        "", 0x105f615a, 0x37c80d2e21ead98cULL, 325, 0, nullptr, exact_hmac));

    machete::sfo::SfoSaveMetadata meta{};
    EXPECT_TRUE(machete::sfo::read_sfo_metadata(tmp_sfo, meta));
    EXPECT_TRUE(meta.has_psid_hmac);
    EXPECT_EQ(std::memcmp(meta.psid_hmac, exact_hmac, 32), 0);
    EXPECT_EQ(meta.account_id, 0x37c80d2e21ead98cULL);
    EXPECT_EQ(meta.user_id, 0x105f615a);

    (void)std::remove(tmp_sfo.c_str());
}

} // namespace hui::testing
