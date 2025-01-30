import * as anchor from "@coral-xyz/anchor";
import { Program } from "@coral-xyz/anchor";
import { VotingSys } from "../target/types/voting_sys";
import { Keypair, PublicKey } from "@solana/web3.js";
import { Transaction } from "@solana/web3.js";
//import { expect } from "chai";

describe("voting-sys", () => {
  // Configure the client to use the local cluster
  const provider = anchor.AnchorProvider.env();
  anchor.setProvider(provider);

  const program = anchor.workspace.VotingSys as Program<VotingSys>;

  before(async () => {
    // No need to airdrop since we're using the default wallet
  });

  it("Complete election process with candidate hash lookup", async () => {
    // Setup new election PDA
    const testElectionPDA = new Keypair();
    
    // Create election with 5 voters and 2 candidates
    await program.methods
      .createElection(new anchor.BN(5), new anchor.BN(2))
      .accounts({
        electionData: testElectionPDA.publicKey,
      })
      .signers([testElectionPDA])
      .rpc();

    const candidates = ["Candidate1", "Candidate2"];
    const candidateAccounts = [];
    const transaction = new Transaction();

    // Add whitelist instructions
    for (const candidate of candidates) {
      transaction.add(
        await program.methods
          .addToCandidateWhitelist(candidate)
          .accounts({
            electionData: testElectionPDA.publicKey,
            initiator: provider.wallet.publicKey,
          })
          .instruction()
      );
    }
    
    // Add register instructions
    for (const candidate of candidates) {
      const [candidateAccount] = PublicKey.findProgramAddressSync(
        [
          Buffer.from("candidate"),
          testElectionPDA.publicKey.toBuffer(),
          Buffer.from(candidate),
        ],
        program.programId
      );
      candidateAccounts.push(candidateAccount);
      transaction.add(
        await program.methods
          .registerCandidate(candidate)
          .accounts({
            candidateData: candidateAccount,
            electionData: testElectionPDA.publicKey,
            signer: provider.wallet.publicKey,
          })
          .instruction()
      );
    }
    
    // Send transaction
    await provider.sendAndConfirm(transaction);

    // Add voters to whitelist
    const voters = ["voter1", "voter2", "voter3", "voter4", "voter5"];
    for (const voter of voters) {
      await program.methods
        .addToVoterWhitelist(voter)
        .accounts({
          electionData: testElectionPDA.publicKey,
          initiator: provider.wallet.publicKey,
        })
        .signers([])
        .rpc();
    }

    // Change to voting stage
    await program.methods
      .changeStage({ voting: {} })
      .accounts({
        electionData: testElectionPDA.publicKey,
        initiator: provider.wallet.publicKey,
      })
      .signers([])
      .rpc();

    // Cast votes: 3 votes for Candidate1, 2 votes for Candidate2
    const voteDistribution = [
      { voter: "voter1", candidate: "Candidate1" },
      { voter: "voter2", candidate: "Candidate1" },
      { voter: "voter3", candidate: "Candidate1" },
      { voter: "voter4", candidate: "Candidate2" },
      { voter: "voter5", candidate: "Candidate2" },
    ];

    for (const vote of voteDistribution) {
      const [voterAccount] = PublicKey.findProgramAddressSync(
        [
          Buffer.from("voter"),
          testElectionPDA.publicKey.toBuffer(),
          Buffer.from(vote.voter),
        ],
        program.programId
      );

      const [candidateAccount] = PublicKey.findProgramAddressSync(
        [
          Buffer.from("candidate"),
          testElectionPDA.publicKey.toBuffer(),
          Buffer.from(vote.candidate),
        ],
        program.programId
      );

      await program.methods
        .vote(vote.voter, vote.candidate)
        .accounts({
          voterData: voterAccount,
          candidateData: candidateAccount,
          electionData: testElectionPDA.publicKey,
          signer: provider.wallet.publicKey,
        })
        .signers([])
        .rpc();
    }

    // Close election
    await program.methods
      .changeStage({ closed: {} })
      .accounts({
        electionData: testElectionPDA.publicKey,
        initiator: provider.wallet.publicKey,
      })
      .signers([])
      .rpc();

    // Verify final vote counts using candidate accounts
    for (let i = 0; i < candidates.length; i++) {
      const candidateData = await program.account.candidateData.fetch(
        candidateAccounts[i],
      );
      console.log(candidateAccounts[i].toString());
      console.log(`${candidates[i]} final vote count: ${candidateData.votes.toString()}`);
    }
  });

});
