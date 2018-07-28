#ifndef NTP1SCRIPT_ISSUANCE_H
#define NTP1SCRIPT_ISSUANCE_H

#include "ntp1script.h"

class NTP1Script_Issuance : public NTP1Script
{
    std::string   tokenSymbol;
    std::string   metadata;
    uint64_t      amount;
    IssuanceFlags issuanceFlags;

    friend class NTP1Script;

    std::string __getAggregAndLockStatusTokenIDHexValue() const;

protected:
    std::vector<TransferInstruction> transferInstructions;

public:
    NTP1Script_Issuance();

    std::string getHexMetadata() const;
    std::string getRawMetadata() const;

    int                                         getDivisibility() const;
    bool                                        isLocked() const;
    IssuanceFlags::AggregationPolicy            getAggregationPolicy() const;
    std::string                                 getAggregationPolicyStr() const;
    std::string                                 getTokenSymbol() const;
    uint64_t                                    getAmount() const;
    unsigned                                    getTransferInstructionsCount() const;
    TransferInstruction                         getTransferInstruction(unsigned index) const;
    std::vector<TransferInstruction>            getTransferInstructions() const;
    static std::shared_ptr<NTP1Script_Issuance> ParseIssuancePostHeaderData(std::string ScriptBin,
                                                                            std::string OpCodeBin);
    std::string getTokenID(std::string input0txid, unsigned int input0index) const;
    std::string calculateScriptBin() const;

    static std::shared_ptr<NTP1Script_Issuance>
                       CreateScript(const std::string& Symbol, uint64_t amount, const std::string& Metadata, bool locked,
                                    unsigned int divisibility, IssuanceFlags::AggregationPolicy aggrPolicy);
    static std::string Create_OpCodeFromMetadata(const std::string& metadata);
    static std::string Create_ProcessTokenSymbol(const std::string& symbol);
};

#endif // NTP1SCRIPT_ISSUANCE_H
