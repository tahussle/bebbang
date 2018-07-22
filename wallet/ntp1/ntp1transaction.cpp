#include "ntp1transaction.h"

#include "ntp1/ntp1script.h"
#include "ntp1/ntp1script_burn.h"
#include "ntp1/ntp1script_issuance.h"
#include "ntp1/ntp1script_transfer.h"
#include "ntp1tools.h"
#include "ntp1txin.h"
#include "ntp1txout.h"
#include "util.h"

#include <boost/algorithm/hex.hpp>

NTP1Transaction::NTP1Transaction() { setNull(); }

void NTP1Transaction::setNull()
{
    nVersion = NTP1Transaction::CURRENT_VERSION;
    nTime    = GetAdjustedTime();
    vin.clear();
    vout.clear();
    nLockTime = 0;
}

bool NTP1Transaction::isNull() const { return (vin.empty() && vout.empty()); }

void NTP1Transaction::importJsonData(const std::string& data)
{
    try {
        json_spirit::Value parsedData;
        json_spirit::read_or_throw(data, parsedData);

        setHex(NTP1Tools::GetStrField(parsedData.get_obj(), "hex"));
        std::string hash = NTP1Tools::GetStrField(parsedData.get_obj(), "txid");
        txHash.SetHex(hash);
        nLockTime                   = NTP1Tools::GetUint64Field(parsedData.get_obj(), "locktime");
        nTime                       = NTP1Tools::GetUint64Field(parsedData.get_obj(), "time");
        nVersion                    = NTP1Tools::GetUint64Field(parsedData.get_obj(), "version");
        json_spirit::Array vin_list = NTP1Tools::GetArrayField(parsedData.get_obj(), "vin");
        vin.clear();
        vin.resize(vin_list.size());
        for (unsigned long i = 0; i < vin_list.size(); i++) {
            vin[i].importJsonData(vin_list[i]);
        }
        json_spirit::Array vout_list = NTP1Tools::GetArrayField(parsedData.get_obj(), "vout");
        vout.clear();
        vout.resize(vout_list.size());
        for (unsigned long i = 0; i < vout_list.size(); i++) {
            vout[i].importJsonData(vout_list[i]);
        }
    } catch (std::exception& ex) {
        printf("%s", ex.what());
        throw;
    }
}

json_spirit::Value NTP1Transaction::exportDatabaseJsonData() const
{
    json_spirit::Object root;

    root.push_back(json_spirit::Pair("version", nVersion));
    root.push_back(json_spirit::Pair("txid", txHash.GetHex()));
    root.push_back(json_spirit::Pair("locktime", nLockTime));
    root.push_back(json_spirit::Pair("time", nTime));
    root.push_back(json_spirit::Pair("hex", getHex()));

    json_spirit::Array vinArray;
    for (long i = 0; i < static_cast<long>(vin.size()); i++) {
        vinArray.push_back(vin[i].exportDatabaseJsonData());
    }
    root.push_back(json_spirit::Pair("vin", json_spirit::Value(vinArray)));

    json_spirit::Array voutArray;
    for (long i = 0; i < static_cast<long>(vout.size()); i++) {
        voutArray.push_back(vout[i].exportDatabaseJsonData());
    }
    root.push_back(json_spirit::Pair("vout", json_spirit::Value(voutArray)));

    return json_spirit::Value(root);
}

void NTP1Transaction::importDatabaseJsonData(const json_spirit::Value& data)
{
    setNull();

    nVersion = NTP1Tools::GetUint64Field(data.get_obj(), "version");
    txHash.SetHex(NTP1Tools::GetStrField(data.get_obj(), "txid"));
    nLockTime = NTP1Tools::GetUint64Field(data.get_obj(), "locktime");
    nTime     = NTP1Tools::GetUint64Field(data.get_obj(), "time");
    setHex(NTP1Tools::GetStrField(data.get_obj(), "hex"));

    json_spirit::Array vin_list = NTP1Tools::GetArrayField(data.get_obj(), "vin");
    vin.clear();
    vin.resize(vin_list.size());
    for (unsigned long i = 0; i < vin_list.size(); i++) {
        vin[i].importDatabaseJsonData(vin_list[i]);
    }

    json_spirit::Array vout_list = NTP1Tools::GetArrayField(data.get_obj(), "vout");
    vout.clear();
    vout.resize(vout_list.size());
    for (unsigned long i = 0; i < vout_list.size(); i++) {
        vout[i].importDatabaseJsonData(vout_list[i]);
    }
}

std::string NTP1Transaction::getHex() const
{
    std::string out;
    boost::algorithm::hex(txSerialized.begin(), txSerialized.end(), std::back_inserter(out));
    return out;
}

void NTP1Transaction::setHex(const std::string& Hex)
{
    txSerialized.clear();
    boost::algorithm::unhex(Hex.begin(), Hex.end(), std::back_inserter(txSerialized));
}

uint256 NTP1Transaction::getTxHash() const { return txHash; }

uint64_t NTP1Transaction::getLockTime() const { return nLockTime; }

uint64_t NTP1Transaction::getTime() const { return nTime; }

unsigned long NTP1Transaction::getTxInCount() const { return vin.size(); }

const NTP1TxIn& NTP1Transaction::getTxIn(unsigned long index) const { return vin[index]; }

unsigned long NTP1Transaction::getTxOutCount() const { return vout.size(); }

const NTP1TxOut& NTP1Transaction::getTxOut(unsigned long index) const { return vout[index]; }

void NTP1Transaction::readNTP1DataFromTx(const CTransaction& tx)
{
    std::string opReturnArg;
    if (!IsTxNTP1(&tx, &opReturnArg)) {
        ntp1TransactionType = NTP1TxType_NOT_NTP1;
        return;
    }

    for (auto&& in : vin) {
        in.tokens.clear();
    }
    vin.resize(tx.vin.size());
    for (auto&& out : vout) {
        out.tokens.clear();
    }
    vout.resize(tx.vout.size());

    txHash = tx.GetHash();

    std::shared_ptr<NTP1Script> scriptPtr = NTP1Script::ParseScript(opReturnArg);
    if (scriptPtr->getTxType() == NTP1Script::TxType::TxType_Issuance) {
        ntp1TransactionType = NTP1TxType_ISSUANCE;
        std::shared_ptr<NTP1Script_Issuance> scriptPtrD =
            std::dynamic_pointer_cast<NTP1Script_Issuance>(scriptPtr);
        uint64_t totalAmountLeft = scriptPtrD->getAmount();
        for (long i = 0; i < scriptPtrD->getTransferInstructionsCount(); i++) {
            NTP1TokenTxData ntp1tokenTxData;
            const auto&     instruction = scriptPtrD->getTransferInstruction(i);
            if (instruction.outputIndex >= static_cast<int>(tx.vout.size())) {
                throw std::runtime_error("An output of issuance is outside the available range of "
                                         "outputs in NTP1 OP_RETURN argument: " +
                                         opReturnArg + ", where the number of available outputs is " +
                                         ::ToString(tx.vout.size()) + " in transaction " +
                                         tx.GetHash().ToString());
            }
            uint64_t currentAmount = instruction.amount;

            // ensure the output is larger than input
            if (totalAmountLeft < currentAmount) {
                throw std::runtime_error("The amount targeted to outputs in bigger than the amount "
                                         "issued in NTP1 OP_RETURN argument: " +
                                         opReturnArg);
            }

            totalAmountLeft -= currentAmount;
            ntp1tokenTxData.setAmount(currentAmount);
            ntp1tokenTxData.setAggregationPolicy(scriptPtrD->getAggregationPolicyStr());
            ntp1tokenTxData.setDivisibility(scriptPtrD->getDivisibility());
            ntp1tokenTxData.setTokenSymbol(scriptPtrD->getTokenSymbol());
            ntp1tokenTxData.setIssueTxIdHex(tx.GetHash().ToString());
            // TODO: fill these
            //            ntp1tokenTxData.setTokenIdBase58();
            vout[instruction.outputIndex].tokens.push_back(ntp1tokenTxData);
        }

    } else if (scriptPtr->getTxType() == NTP1Script::TxType::TxType_Transfer) {
        ntp1TransactionType = NTP1TxType_TRANSFER;
        std::shared_ptr<NTP1Script_Transfer> scriptPtrD =
            std::dynamic_pointer_cast<NTP1Script_Transfer>(scriptPtr);
        int currentInputIndex = 0;
        for (long i = 0; i < scriptPtrD->getTransferInstructionsCount(); i++) {
            const auto& instruction = scriptPtrD->getTransferInstruction(i);

            // if skip, move on to the next input
            if (instruction.skipInput) {
                currentInputIndex++;
                continue;
            }
            if (currentInputIndex >= static_cast<int>(vin.size())) {
                throw std::runtime_error("An input of transfer instruction is outside the available "
                                         "range of inputs in NTP1 OP_RETURN argument: " +
                                         opReturnArg + ", where the number of available inputs is " +
                                         ::ToString(tx.vin.size()) + " in transaction " +
                                         tx.GetHash().ToString());
            }
            int outputIndex = instruction.outputIndex;
            if (outputIndex >= static_cast<int>(vout.size())) {
                throw std::runtime_error("An output of transfer instruction is outside the available "
                                         "range of outputs in NTP1 OP_RETURN argument: " +
                                         opReturnArg + ", where the number of available outputs is " +
                                         ::ToString(tx.vout.size()) + " in transaction " +
                                         tx.GetHash().ToString());
            }
            uint64_t currentAmount = instruction.amount;

            NTP1TokenTxData ntp1tokenTxData;
            ntp1tokenTxData.setAmount(currentAmount);

            // we set only the amount because the other stuff is still unknown
            // we don't set inputs because that's still unknown from OP_RETURN
            vout[outputIndex].tokens.push_back(ntp1tokenTxData);

            currentInputIndex++;
        }
    } else if (scriptPtr->getTxType() == NTP1Script::TxType::TxType_Burn) {
        ntp1TransactionType = NTP1TxType_BURN;
        std::shared_ptr<NTP1Script_Burn> scriptPtrD =
            std::dynamic_pointer_cast<NTP1Script_Burn>(scriptPtr);
        int currentInputIndex = 0;
        for (long i = 0; i < scriptPtrD->getTransferInstructionsCount(); i++) {
            const auto& instruction = scriptPtrD->getTransferInstruction(i);

            // if skip, move on to the next input
            if (instruction.skipInput) {
                currentInputIndex++;
                continue;
            }
            if (currentInputIndex >= static_cast<int>(vin.size())) {
                throw std::runtime_error("An input of transfer instruction is outside the available "
                                         "range of inputs in NTP1 OP_RETURN argument: " +
                                         opReturnArg + ", where the number of available inputs is " +
                                         ::ToString(tx.vin.size()) + " in transaction " +
                                         tx.GetHash().ToString());
            }
            int outputIndex = instruction.outputIndex;
            // output 31 is for burning
            if (outputIndex == 31) {
                currentInputIndex++;
                continue;
            }
            if (outputIndex >= static_cast<int>(vout.size())) {
                throw std::runtime_error("An output of transfer instruction is outside the available "
                                         "range of outputs in NTP1 OP_RETURN argument: " +
                                         opReturnArg + ", where the number of available outputs is " +
                                         ::ToString(tx.vout.size()) + " in transaction " +
                                         tx.GetHash().ToString());
            }
            uint64_t currentAmount = instruction.amount;

            NTP1TokenTxData ntp1tokenTxData;
            ntp1tokenTxData.setAmount(currentAmount);

            // we set only the amount because the other stuff is still unknown
            // we don't set inputs because that's still unknown from OP_RETURN
            vout[outputIndex].tokens.push_back(ntp1tokenTxData);

            currentInputIndex++;
        }

    } else {
        ntp1TransactionType = NTP1TxType_INVALID;
        throw std::runtime_error("Unknown NTP1 transaction type");
    }
}

bool NTP1Transaction::writeToDisk(unsigned int& nFileRet, unsigned int& nTxPosRet)
{
    // Open history file to append
    CAutoFile fileout = CAutoFile(AppendNTP1TxsFile(nFileRet), SER_DISK, CLIENT_VERSION);
    if (!fileout)
        return error("NTP1Transaction::WriteToDisk() : AppendNTP1TxsFile failed");

    // Write tx
    long fileOutPos = ftell(fileout);
    if (fileOutPos < 0)
        return error("NTP1Transaction::WriteToDisk() : ftell failed");
    nTxPosRet = fileOutPos;
    fileout << *this;

    // Flush stdio buffers and commit to disk before returning
    fflush(fileout);
    FileCommit(fileout);

    return true;
}

bool NTP1Transaction::readFromDisk(DiskNTP1TxPos pos, FILE** pfileRet)
{
    CAutoFile filein =
        CAutoFile(OpenBlockFile(pos.nFile, 0, pfileRet ? "rb+" : "rb"), SER_DISK, CLIENT_VERSION);
    if (!filein)
        return error("NTP1Transaction::ReadFromDisk() : OpenBlockFile failed");

    // Read transaction
    if (fseek(filein, pos.nTxPos, SEEK_SET) != 0)
        return error("NTP1Transaction::ReadFromDisk() : fseek failed");

    try {
        filein >> *this;
    } catch (std::exception& e) {
        return error("%s() : deserialize or I/O error", __PRETTY_FUNCTION__);
    }

    // Return file pointer
    if (pfileRet) {
        if (fseek(filein, pos.nTxPos, SEEK_SET) != 0)
            return error("NTP1Transaction::ReadFromDisk() : second fseek failed");
        *pfileRet = filein.release();
    }
    return true;
}
