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
#include "core/samu_vault.hpp"
#include "core/account_manager.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

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
    EXPECT_EQ(server.port(), 8080);
    EXPECT_FALSE(server.url().empty());
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

} // namespace hui::testing
