#include "cc/eval.h"
#include "main.h"
#include "notarisationdb.h"


/* On KMD */
uint256 GetProofRoot(char* symbol, uint32_t targetCCid, int kmdHeight, std::vector<uint256> &moms, int* assetChainHeight)
{
    /*
     * Notaries don't wait for confirmation on KMD before performing a backnotarisation,
     * but we need a determinable range that will encompass all merkle roots. Include MoMs
     * including the block height of the last notarisation until the height before the
     * previous notarisation.
     */
    if (targetCCid <= 1)
        return uint256();

    int seenOwnNotarisations = 0;

    // TODO: test height out of range
    // TODO: Make sure that boundary for moms is notarisation tx not block

    for (int i=0; i<1440; i++) {
        if (i > kmdHeight) break;
        NotarisationsInBlock notarisations;
        uint256 blockHash = *chainActive[kmdHeight-i]->phashBlock;
        if (!pnotarisations->Read(blockHash, notarisations))
            continue;
        BOOST_FOREACH(Notarisation& nota, notarisations) {
            NotarisationData& data = nota.second;
            if (data.ccId != targetCCid)
                continue;
            if (strcmp(data.symbol, symbol) == 0)
            {
                seenOwnNotarisations++;
                if (seenOwnNotarisations == 2)
                    goto end;
                if (seenOwnNotarisations == 1)
                    *assetChainHeight = data.height;  // TODO: Needed?
                continue;  // Don't include own MoMs
            }
            if (seenOwnNotarisations == 1)
                moms.push_back(data.MoM);
        }
    }

end:
    return GetMerkleRoot(moms);
}


/* On KMD */
std::pair<uint256,MerkleBranch> GetCrossChainProof(uint256 txid, char* targetSymbol,
        uint32_t targetCCid, uint256 notarisationTxid, MerkleBranch assetChainProof)
{
    /*
     * Here we are given a proof generated by an assetchain A which goes from given txid to
     * an assetchain MoM. We need to go from the notarisationTxid for A to the MoMoM range of the
     * backnotarisation for B (given by kmdheight of notarisation), find the MoM within the MoMs for
     * that range, and finally extend the proof to lead to the MoMoM (proof root).
     */
    EvalRef eval;
    uint256 MoM = assetChainProof.Exec(txid);
    
    // Get a kmd height for given notarisation Txid
    int kmdHeight;
    {
        CTransaction sourceNotarisation;
        uint256 hashBlock;
        CBlockIndex blockIdx;
        if (eval->GetTxConfirmed(notarisationTxid, sourceNotarisation, blockIdx))
            kmdHeight = blockIdx.nHeight;
        else if (eval->GetTxUnconfirmed(notarisationTxid, sourceNotarisation, hashBlock))
            kmdHeight = chainActive.Tip()->nHeight;
        else
            throw std::runtime_error("Notarisation not found");
    }

    // Get MoMs for kmd height and symbol
    std::vector<uint256> moms;
    int targetChainStartHeight;
    uint256 MoMoM = GetProofRoot(targetSymbol, targetCCid, kmdHeight, moms, &targetChainStartHeight);
    if (MoMoM.IsNull())
        throw std::runtime_error("No MoMs found");
    
    // Find index of source MoM in MoMoM
    int nIndex;
    for (nIndex=0; nIndex<moms.size(); nIndex++)
        if (moms[nIndex] == MoM)
            goto cont;
    throw std::runtime_error("Couldn't find MoM within MoMoM set");
cont:

    // Create a branch
    std::vector<uint256> newBranch;
    {
        CBlock fakeBlock;
        for (int i=0; i<moms.size(); i++) {
            CTransaction fakeTx;
            // first value in CTransaction memory is it's hash
            memcpy((void*)&fakeTx, moms[i].begin(), 32);
            fakeBlock.vtx.push_back(fakeTx);
        }
        newBranch = fakeBlock.GetMerkleBranch(nIndex);
    }

    // Concatenate branches
    MerkleBranch newProof = assetChainProof;
    newProof << MerkleBranch(nIndex, newBranch);

    // Check proof
    if (newProof.Exec(txid) != MoMoM)
        throw std::runtime_error("Proof check failed");

    return std::make_pair(uint256(), newProof);
}


/* On assetchain */
bool ValidateCrossChainProof(uint256 txid, int notarisationHeight, MerkleBranch proof)
{
    /*
     * Here we are given a notarisation txid, and a proof.
     * We go from the notarisation to get the backnotarisation, and verify the proof
     * against the MoMoM it contains.
     */
}



int32_t komodo_MoM(int32_t *notarized_htp,uint256 *MoMp,uint256 *kmdtxidp,int32_t nHeight,uint256 *MoMoMp,int32_t *MoMoMoffsetp,int32_t *MoMoMdepthp,int32_t *kmdstartip,int32_t *kmdendip);

/*
 * On assetchain
 * in: txid
 * out: pair<notarisationTxHash,merkleBranch>
 */
std::pair<uint256,MerkleBranch> GetAssetchainProof(uint256 hash)
{
    uint256 notarisationHash, MoM,MoMoM; int32_t notarisedHeight, depth; CBlockIndex* blockIndex;
    std::vector<uint256> branch;
    int nIndex,MoMoMdepth,MoMoMoffset,kmdstarti,kmdendi;

    {
        uint256 blockHash;
        CTransaction tx;
        if (!GetTransaction(hash, tx, blockHash, true))
            throw std::runtime_error("cannot find transaction");

        blockIndex = mapBlockIndex[blockHash];

        depth = komodo_MoM(&notarisedHeight, &MoM, &notarisationHash, blockIndex->nHeight,&MoMoM,&MoMoMoffset,&MoMoMdepth,&kmdstarti,&kmdendi);

        if (!depth)
            throw std::runtime_error("notarisation not found");
        
        // index of block in MoM leaves
        nIndex = notarisedHeight - blockIndex->nHeight;
    }

    // build merkle chain from blocks to MoM
    {
        // since the merkle branch code is tied up in a block class
        // and we want to make a merkle branch for something that isnt transactions
        CBlock fakeBlock;
        for (int i=0; i<depth; i++) {
            uint256 mRoot = chainActive[notarisedHeight - i]->hashMerkleRoot;
            CTransaction fakeTx;
            // first value in CTransaction memory is it's hash
            memcpy((void*)&fakeTx, mRoot.begin(), 32);
            fakeBlock.vtx.push_back(fakeTx);
        }
        branch = fakeBlock.GetMerkleBranch(nIndex);

        // Check branch
        if (MoM != CBlock::CheckMerkleBranch(blockIndex->hashMerkleRoot, branch, nIndex))
            throw std::runtime_error("Failed merkle block->MoM");
    }

    // Now get the tx merkle branch
    {
        CBlock block;

        if (fHavePruned && !(blockIndex->nStatus & BLOCK_HAVE_DATA) && blockIndex->nTx > 0)
            throw std::runtime_error("Block not available (pruned data)");

        if(!ReadBlockFromDisk(block, blockIndex,1))
            throw std::runtime_error("Can't read block from disk");

        // Locate the transaction in the block
        int nTxIndex;
        for (nTxIndex = 0; nTxIndex < (int)block.vtx.size(); nTxIndex++)
            if (block.vtx[nTxIndex].GetHash() == hash)
                break;

        if (nTxIndex == (int)block.vtx.size())
            throw std::runtime_error("Error locating tx in block");

        std::vector<uint256> txBranch = block.GetMerkleBranch(nTxIndex);

        // Check branch
        if (block.hashMerkleRoot != CBlock::CheckMerkleBranch(hash, txBranch, nTxIndex))
            throw std::runtime_error("Failed merkle tx->block");

        // concatenate branches
        nIndex = (nIndex << txBranch.size()) + nTxIndex;
        branch.insert(branch.begin(), txBranch.begin(), txBranch.end());
    }

    // Check the proof
    if (MoM != CBlock::CheckMerkleBranch(hash, branch, nIndex)) 
        throw std::runtime_error("Failed validating MoM");

    // All done!
    CDataStream ssProof(SER_NETWORK, PROTOCOL_VERSION);
    return std::make_pair(notarisationHash, MerkleBranch(nIndex, branch));
}
