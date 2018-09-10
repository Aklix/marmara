/******************************************************************************
 * Copyright © 2014-2018 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#ifndef CC_INCLUDE_H
#define CC_INCLUDE_H

/*
there are only a very few types in bitcoin. pay to pubkey, pay to pubkey hash and pay to script hash
p2pk, p2pkh, p2sh
there are actually more that are possible, but those three are 99%+ of bitcoin transactions
so you can pay to a pubkey, or to its hash. or to a script's hash. the last is how most of the more complex scripts are invoked. to spend a p2sh vout, you need to provide the redeemscript, this script's hash is what the p2sh address was.
all of the above are the standard bitcoin vout types and there should be plenty of materials about it
Encrypted by a verified device
what I did with the CC contracts is created a fourth type of vout, the CC vout. this is using the cryptoconditions standard and it is even a different signature mechanism. ed25519 instead of secp256k1. it is basically a big extension to the bitcoin script. There is a special opcode that is added that says it is a CC script.
 
but it gets more interesting
each CC script has an evalcode
this is just an arbitrary number. but what it does is allows to create a self-contained universe of CC utxo that all have the same evalcode and that is how a faucet CC differentiates itself from a dice CC, the eval code is different

one effect from using a different eval code is that even if the rest of the CC script is the same, the bitcoin address that is calculated is different. what this means is that for each pubkey, there is a unique address for each different eval code!
and this allows efficient segregation of one CC contracts transactions from another
the final part that will make it all clear how the funds can be locked inside the contract. this is what makes a contract, a contract. I put both the privkey and pubkey for a randomly chosen address and associate it with each CC contract. That means anybody can sign outputs for that privkey. However, it is a CC output, so in addition to the signature, whatever constraints a CC contract implements must also be satistifed. This allows funds to be locked and yet anybody is able to spend it, assuming they satisfy the CC's rules

one other technical note is that komodod has the insight-explorer extensions built in. so it can lookup directly all transactions to any address. this is a key performance boosting thing as if it wasnt there, trying to get all the utxo for an address not in the wallet is quite time consuming
*/

#include <cc/eval.h>
#include <script/cc.h>
#include <script/script.h>
#include <cryptoconditions.h>
#include "../script/standard.h"
#include "../base58.h"
#include "../core_io.h"
#include "../script/sign.h"
#include "../wallet/wallet.h"
#include <univalue.h>
#include <exception>
#include "../komodo_defs.h"

extern int32_t KOMODO_CONNECTING,KOMODO_CCACTIVATE;
extern uint32_t ASSETCHAINS_CC;
extern std::string CCerror;

#define SMALLVAL 0.000000000000001
union _bits256 { uint8_t bytes[32]; uint16_t ushorts[16]; uint32_t uints[8]; uint64_t ulongs[4]; uint64_t txid; };
typedef union _bits256 bits256;

struct CC_utxo
{
    uint256 txid;
    int64_t nValue;
    int32_t vout;
};

struct CCcontract_info
{
    uint256 prevtxid;
    char unspendableCCaddr[64],CChexstr[72],normaladdr[64],unspendableaddr2[64];
    uint8_t CCpriv[32],unspendablepriv2[32];
    CPubKey unspendablepk2;
    bool (*validate)(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx);
    bool (*ismyvin)(CScript const& scriptSig);
    uint8_t evalcode,didinit;
};
struct CCcontract_info *CCinit(struct CCcontract_info *cp,uint8_t evalcode);

#ifdef ENABLE_WALLET
extern CWallet* pwalletMain;
#endif
bool GetAddressUnspent(uint160 addressHash, int type,std::vector<std::pair<CAddressUnspentKey,CAddressUnspentValue> > &unspentOutputs);

static const uint256 zeroid;
bool myGetTransaction(const uint256 &hash, CTransaction &txOut, uint256 &hashBlock);
int32_t is_hexstr(char *str,int32_t n);
bool myAddtomempool(CTransaction &tx);
//uint64_t myGettxout(uint256 hash,int32_t n);
bool myIsutxo_spentinmempool(uint256 txid,int32_t vout);
int32_t myIsutxo_spent(uint256 &spenttxid,uint256 txid,int32_t vout);
bool mySendrawtransaction(std::string res);
int32_t decode_hex(uint8_t *bytes,int32_t n,char *hex);
int32_t iguana_rwnum(int32_t rwflag,uint8_t *serialized,int32_t len,void *endianedp);
int32_t iguana_rwbignum(int32_t rwflag,uint8_t *serialized,int32_t len,uint8_t *endianedp);

int64_t OraclePrice(int32_t height,uint256 reforacletxid,char *markeraddr,char *format);

// CCcustom
CPubKey GetUnspendable(struct CCcontract_info *cp,uint8_t *unspendablepriv);

// CCutils
CPubKey buf2pk(uint8_t *buf33);
void endiancpy(uint8_t *dest,uint8_t *src,int32_t len);
uint256 DiceHashEntropy(uint256 &entropy,uint256 _txidpriv);
CTxOut MakeCC1vout(uint8_t evalcode,CAmount nValue,CPubKey pk);
CTxOut MakeCC1of2vout(uint8_t evalcode,CAmount nValue,CPubKey pk,CPubKey pk2);
CC *MakeCCcond1(uint8_t evalcode,CPubKey pk);
CC* GetCryptoCondition(CScript const& scriptSig);
bool IsCCInput(CScript const& scriptSig);
int32_t unstringbits(char *buf,uint64_t bits);
uint64_t stringbits(char *str);
uint256 revuint256(uint256 txid);
char *uint256_str(char *dest,uint256 txid);
char *pubkey33_str(char *dest,uint8_t *pubkey33);
uint256 Parseuint256(char *hexstr);
CPubKey pubkey2pk(std::vector<uint8_t> pubkey);
bool GetCCaddress(struct CCcontract_info *cp,char *destaddr,CPubKey pk);
bool GetCCaddress1of2(struct CCcontract_info *cp,char *destaddr,CPubKey pk,CPubKey pk2);
bool ConstrainVout(CTxOut vout,int32_t CCflag,char *cmpaddr,int64_t nValue);
bool PreventCC(Eval* eval,const CTransaction &tx,int32_t preventCCvins,int32_t numvins,int32_t preventCCvouts,int32_t numvouts);
bool Getscriptaddress(char *destaddr,const CScript &scriptPubKey);
std::vector<uint8_t> Mypubkey();
bool Myprivkey(uint8_t myprivkey[]);
int64_t CCduration(int32_t &numblocks,uint256 txid);

// CCtx
std::string FinalizeCCTx(uint64_t skipmask,struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey mypk,uint64_t txfee,CScript opret);
void SetCCunspents(std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs,char *coinaddr);
void SetCCtxids(std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex,char *coinaddr);
int64_t AddNormalinputs(CMutableTransaction &mtx,CPubKey mypk,int64_t total,int32_t maxinputs);
int64_t CCutxovalue(char *coinaddr,uint256 utxotxid,int32_t utxovout);

// curve25519 and sha256
bits256 curve25519_shared(bits256 privkey,bits256 otherpub);
bits256 curve25519_basepoint9();
bits256 curve25519(bits256 mysecret,bits256 basepoint);
void vcalc_sha256(char deprecated[(256 >> 3) * 2 + 1],uint8_t hash[256 >> 3],uint8_t *src,int32_t len);
bits256 bits256_doublesha256(char *deprecated,uint8_t *data,int32_t datalen);

#endif
