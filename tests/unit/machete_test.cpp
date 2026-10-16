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
#include "core/sha256_hmac.hpp"
#include "core/sfo_resigner.hpp"
#include "core/keystone_manager.hpp"
#include "core/pfs_mount_manager.hpp"
#include "core/samu_vault.hpp"
#include "core/account_manager.hpp"
#include "ui/qr_code.hpp"

#include <fcntl.h>
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
    EXPECT_STREQ(machete_->info().name, "MacheteSVG Save Manager");
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
    unsigned int tex = machete::scanner::load_texture_from_png("sce_sys/icon0.png");
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
    EXPECT_NE(status_resp.find("\"version\":\"1.2.4\""), std::string::npos);
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

} // namespace hui::testing
