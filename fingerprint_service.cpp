#include "fingerprint_service.h"
#include "database.h"
#include <iostream>

FingerprintService::FingerprintService(ApiClient& apiClient, sqlite3* db, FingerprintScannerInterface* scanner)
    : apiClient_(apiClient)
    , db_(db)
    , scanner_(scanner)
{
}

bool FingerprintService::startEnrollment(const std::string& memberId)
{
    if (memberId.empty()) {
        std::cerr << "FingerprintService: memberId is empty" << std::endl;
        return false;
    }

    if (!apiClient_.startEnrollment(memberId, db_)) {
        std::cerr << "FingerprintService: failed to request enrollment start" << std::endl;
        return false;
    }

    if (scanner_ && !scanner_->startEnrollment(memberId)) {
        std::cerr << "FingerprintService: scanner failed to start enrollment" << std::endl;
        return false;
    }

    pendingMemberId_ = memberId;
    return true;
}

bool FingerprintService::verifyFingerprint()
{
    if (pendingMemberId_.empty()) {
        std::cerr << "FingerprintService: no pending enrollment" << std::endl;
        return false;
    }

    if (!scanner_) {
        std::cerr << "FingerprintService: fingerprint scanner is unavailable" << std::endl;
        return false;
    }

    std::string memberId;
    if (!scanner_->verifyFingerprint(memberId)) {
        std::cerr << "FingerprintService: scanner verifyFingerprint failed" << std::endl;
        return false;
    }

    if (memberId != pendingMemberId_) {
        std::cerr << "FingerprintService: verified member id mismatch" << std::endl;
        return false;
    }

    std::string fingerprintTemplate = scanner_->captureFingerprintTemplate();
    if (fingerprintTemplate.empty()) {
        std::cerr << "FingerprintService: failed to capture fingerprint" << std::endl;
        return false;
    }

    if (!apiClient_.submitEnrollmentResult(pendingMemberId_, fingerprintTemplate, db_)) {
        std::cerr << "FingerprintService: failed to submit enrollment result" << std::endl;
        return false;
    }

    pendingMemberId_.clear();
    return true;
}
