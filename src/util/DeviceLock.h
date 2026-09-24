#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>

// Startup password: a passphrase asked for at boot and on wake before the UI
// is reachable.
//
// What this is and is not. It keeps someone who picks the device up out of
// your books; it is NOT disk encryption. Everything on the reader lives on a
// removable FAT SD card, so anyone holding the card can read it, and anyone
// who can flash the device can clear this. Treat it as a screen lock.
//
// The passphrase is stored as a SHA-256 hash rather than plaintext -- not
// because a short one resists brute force (it does not; an attacker with the
// card can try every 4-digit PIN instantly), but so a passphrase reused from
// somewhere else isn't sitting in a readable file on a card that gets plugged
// into other computers.
//
// Kept in its own file rather than settings.json so the hash never rides along
// in settings exports, and so upstream's settings list stays untouched.
class DeviceLock : public PersistableStore<DeviceLock> {
  DeviceLock() = default;
  ~DeviceLock() = default;

  friend class PersistableStore<DeviceLock>;

 public:
  static constexpr size_t MIN_LENGTH = 4;
  static constexpr size_t MAX_LENGTH = 32;

  static const char* getFilePath() { return "/.crosspoint/lock.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  // True when a passphrase is set AND locking is switched on.
  bool isLocked() const { return enabled && !hash.empty(); }
  bool hasPassphrase() const { return !hash.empty(); }
  bool lockOnWake() const { return onWake; }

  // Rejects anything shorter than MIN_LENGTH. Saves on success.
  bool setPassphrase(const std::string& passphrase);
  // Clears the passphrase and switches locking off. Saves.
  void clear();
  bool verify(const std::string& passphrase) const;

  void setEnabled(bool on);
  void setLockOnWake(bool on);

 private:
  static std::string hashOf(const std::string& passphrase);

  std::string hash;
  bool enabled = false;
  // Asking again on every wake is the point of a lock for most people, but it
  // is a lot of typing on an e-ink keyboard, so it is separately switchable.
  bool onWake = true;
};

#define DEVICE_LOCK DeviceLock::getInstance()
