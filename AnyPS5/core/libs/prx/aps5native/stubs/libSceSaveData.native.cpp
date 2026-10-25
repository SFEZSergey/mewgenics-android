// Сгенерировано tools/gen_native_stubs.py: x86-заглушка libSceSaveData.native.prx, переадресующая вызовы
// в нативную (aarch64) сборку через мост box64. Не править вручную.
#include "prx/aps5native/X86Host.hpp"

#include <cstdint>
#include <cstring>

namespace {

[[maybe_unused]] double floatBits(float value) {
    // float передаётся в младших 32 битах d-регистра (нативная сторона читает s-регистр).
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(value));
    double result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

const char* const names[] = {
    "sceSaveDataBackup",
    "sceSaveDataCancel",
    "sceSaveDataCommit",
    "sceSaveDataConvert",
    "sceSaveDataCreateTransactionResource",
    "sceSaveDataDelete",
    "sceSaveDataDeleteTransactionResource",
    "sceSaveDataDirNameSearch",
    "sceSaveDataDirNameSearchPs4",
    "sceSaveDataGetConvertProgress",
    "sceSaveDataGetEventResult",
    "sceSaveDataGetMountInfo",
    "sceSaveDataGetParam",
    "sceSaveDataGetSaveDataMemory2",
    "sceSaveDataInitialize3",
    "sceSaveDataLoadIcon",
    "sceSaveDataMount3",
    "sceSaveDataPrepare",
    "sceSaveDataSaveIcon",
    "sceSaveDataSaveIconByPath",
    "sceSaveDataSetParam",
    "sceSaveDataSetSaveDataMemory2",
    "sceSaveDataSetupSaveDataMemory2",
    "sceSaveDataSyncSaveDataMemory",
    "sceSaveDataTerminate",
    "sceSaveDataTransferringMount",
    "sceSaveDataTransferringMountPs4",
    "sceSaveDataUmount2",
};
void* functions[28] = {};

__attribute__((constructor)) void loadNative() {
    Aps5NativeResolve_nid_no_patch("libSceSaveData.native.prx", names, functions, 28);
}

} // namespace

extern "C" std::uint64_t sceSaveDataBackup(std::uint64_t a0) {
    if (functions[0] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataBackup");
    return Aps5NativeCall_nid_no_patch(functions[0], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataCancel() {
    if (functions[1] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataCancel");
    return Aps5NativeCall_nid_no_patch(functions[1], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataCommit(std::uint64_t a0) {
    if (functions[2] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataCommit");
    return Aps5NativeCall_nid_no_patch(functions[2], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataConvert() {
    if (functions[3] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataConvert");
    return Aps5NativeCall_nid_no_patch(functions[3], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataCreateTransactionResource(std::uint64_t a0) {
    if (functions[4] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataCreateTransactionResource");
    return Aps5NativeCall_nid_no_patch(functions[4], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataDelete(std::uint64_t a0) {
    if (functions[5] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataDelete");
    return Aps5NativeCall_nid_no_patch(functions[5], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataDeleteTransactionResource(std::uint64_t a0) {
    if (functions[6] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataDeleteTransactionResource");
    return Aps5NativeCall_nid_no_patch(functions[6], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataDirNameSearch(std::uint64_t a0, std::uint64_t a1) {
    if (functions[7] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataDirNameSearch");
    return Aps5NativeCall_nid_no_patch(functions[7], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataDirNameSearchPs4(std::uint64_t a0, std::uint64_t a1) {
    if (functions[8] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataDirNameSearchPs4");
    return Aps5NativeCall_nid_no_patch(functions[8], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataGetConvertProgress() {
    if (functions[9] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataGetConvertProgress");
    return Aps5NativeCall_nid_no_patch(functions[9], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataGetEventResult(std::uint64_t a0, std::uint64_t a1) {
    if (functions[10] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataGetEventResult");
    return Aps5NativeCall_nid_no_patch(functions[10], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataGetMountInfo(std::uint64_t a0, std::uint64_t a1) {
    if (functions[11] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataGetMountInfo");
    return Aps5NativeCall_nid_no_patch(functions[11], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataGetParam(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4) {
    if (functions[12] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataGetParam");
    return Aps5NativeCall_nid_no_patch(functions[12], a0, a1, a2, a3, a4, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataGetSaveDataMemory2(std::uint64_t a0) {
    if (functions[13] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataGetSaveDataMemory2");
    return Aps5NativeCall_nid_no_patch(functions[13], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataInitialize3(std::uint64_t a0) {
    if (functions[14] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataInitialize3");
    return Aps5NativeCall_nid_no_patch(functions[14], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataLoadIcon(std::uint64_t a0, std::uint64_t a1) {
    if (functions[15] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataLoadIcon");
    return Aps5NativeCall_nid_no_patch(functions[15], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataMount3(std::uint64_t a0, std::uint64_t a1) {
    if (functions[16] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataMount3");
    return Aps5NativeCall_nid_no_patch(functions[16], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataPrepare(std::uint64_t a0, std::uint64_t a1) {
    if (functions[17] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataPrepare");
    return Aps5NativeCall_nid_no_patch(functions[17], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataSaveIcon(std::uint64_t a0, std::uint64_t a1) {
    if (functions[18] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataSaveIcon");
    return Aps5NativeCall_nid_no_patch(functions[18], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataSaveIconByPath(std::uint64_t a0, std::uint64_t a1) {
    if (functions[19] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataSaveIconByPath");
    return Aps5NativeCall_nid_no_patch(functions[19], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataSetParam(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    if (functions[20] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataSetParam");
    return Aps5NativeCall_nid_no_patch(functions[20], a0, a1, a2, a3, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataSetSaveDataMemory2(std::uint64_t a0) {
    if (functions[21] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataSetSaveDataMemory2");
    return Aps5NativeCall_nid_no_patch(functions[21], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataSetupSaveDataMemory2(std::uint64_t a0, std::uint64_t a1) {
    if (functions[22] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataSetupSaveDataMemory2");
    return Aps5NativeCall_nid_no_patch(functions[22], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataSyncSaveDataMemory(std::uint64_t a0) {
    if (functions[23] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataSyncSaveDataMemory");
    return Aps5NativeCall_nid_no_patch(functions[23], a0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataTerminate() {
    if (functions[24] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataTerminate");
    return Aps5NativeCall_nid_no_patch(functions[24], 0, 0, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataTransferringMount(std::uint64_t a0, std::uint64_t a1) {
    if (functions[25] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataTransferringMount");
    return Aps5NativeCall_nid_no_patch(functions[25], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataTransferringMountPs4(std::uint64_t a0, std::uint64_t a1) {
    if (functions[26] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataTransferringMountPs4");
    return Aps5NativeCall_nid_no_patch(functions[26], a0, a1, 0, 0, 0, 0, 0, 0);
}

extern "C" std::uint64_t sceSaveDataUmount2(std::uint64_t a0, std::uint64_t a1) {
    if (functions[27] == nullptr) Aps5NativeMissing_nid_no_patch("sceSaveDataUmount2");
    return Aps5NativeCall_nid_no_patch(functions[27], a0, a1, 0, 0, 0, 0, 0, 0);
}
