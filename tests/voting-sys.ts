import * as anchor from "@coral-xyz/anchor";
import { Program } from "@coral-xyz/anchor";
import { VotingSys } from "../target/types/voting_sys";
import { Keypair, PublicKey } from "@solana/web3.js";
import { expect } from "chai";

describe("voting-sys", () => {
  // Configure the client to use the local cluster
  const provider = anchor.AnchorProvider.env();
  anchor.setProvider(provider);

  const program = anchor.workspace.VotingSys as Program<VotingSys>;

  // Test accounts
  const voter = anchor.web3.Keypair.generate();
  const candidate = anchor.web3.Keypair.generate();

  // Test data
  const totalVotes = new anchor.BN(100);
  const totalCandidates = new anchor.BN(5);
  const voterId = "voter1";
  const candidateName = "candidate1";

  // Generate election PDA
  const electionPDA = new Keypair();
  console.log(electionPDA.publicKey.toString());
  before(async () => {
    // No need to airdrop since we're using the default wallet
  });

  it("Creates an election", async () => {
    const tx = await program.methods
      .createElection(totalVotes, totalCandidates)
      .accounts({
        electionData: electionPDA.publicKey,
      })
      .signers([electionPDA])
      .rpc();

    const electionAccount = await program.account.electionData.fetch(
      electionPDA.publicKey
    );
    console.log(electionAccount.stage);
    expect(electionAccount.stage).to.deep.equal({ application: {} });
    expect(electionAccount.initiator.toString()).to.equal(
      provider.wallet.publicKey.toString()
    );
    expect(electionAccount.totalVotes.toNumber()).to.equal(
      totalVotes.toNumber()
    );
    expect(electionAccount.totalCandidates.toNumber()).to.equal(
      totalCandidates.toNumber()
    );
  });

  it("Adds voter to whitelist", async () => {
    await program.methods
      .addToVoterWhitelist(voterId)
      .accounts({
        electionData: electionPDA.publicKey,
        initiator: provider.wallet.publicKey, // Add the initiator account
      })
      .signers([]) // Remove electionPDA from signers, use provider's wallet which is implicit
      .rpc();

    const electionAccount = await program.account.electionData.fetch(
      electionPDA.publicKey
    );

    expect(electionAccount.stage).to.deep.equal({ application: {} });
    expect(electionAccount.initiator.toString()).to.equal(
      provider.wallet.publicKey.toString()
    );
    expect(electionAccount.voterWhitelist).to.include(voterId);
  });

  it("Adds candidate to whitelist", async () => {
    await program.methods
      .addToCandidateWhitelist(candidateName)
      .accounts({
        electionData: electionPDA.publicKey,
        initiator: provider.wallet.publicKey,
      })
      .signers([])
      .rpc();

    const electionAccount = await program.account.electionData.fetch(
      electionPDA.publicKey
    );
    expect(electionAccount.stage).to.deep.equal({ application: {} });
    expect(electionAccount.initiator.toString()).to.equal(
      provider.wallet.publicKey.toString()
    );
    expect(electionAccount.candidateWhitelist).to.include(candidateName);
  });

  it("Registers a candidate", async () => {
    const [candidateAccount, _bump] = PublicKey.findProgramAddressSync(
      [
        Buffer.from("candidate"),
        electionPDA.publicKey.toBuffer(),
        Buffer.from(candidateName),
      ],
      program.programId
    );

    await program.methods
      .registerCandidate(candidateName)
      .accounts({
        candidateData: candidateAccount,
        electionData: electionPDA.publicKey,
        signer: provider.wallet.publicKey,
      })
      .signers([])
      .rpc();

    const candidateData = await program.account.candidateData.fetch(
      candidateAccount
    );
    const electionAccount = await program.account.electionData.fetch(
      electionPDA.publicKey
    );
    expect(electionAccount.stage).to.deep.equal({ application: {} });
    expect(candidateData.votes.toNumber()).to.equal(0);
  });

  it("Should prevent adding duplicate voters to whitelist", async () => {
    // already whitelisted
    try {
      await program.methods
        .addToVoterWhitelist(voterId)
        .accounts({
          electionData: electionPDA.publicKey,
          initiator: provider.wallet.publicKey,
        })
        .signers([])
        .rpc();
      expect.fail("Voter is already whitelisted.");
    } catch (error) {
      expect(error.message).to.include("Voter is already whitelisted.");
    }
  });


  it("Changes election stage to voting", async () => {
    await program.methods
      .changeStage({ voting: {} })
      .accounts({
        electionData: electionPDA.publicKey,
        initiator: provider.wallet.publicKey,
      })
      .signers([])
      .rpc();

    const electionAccount = await program.account.electionData.fetch(
      electionPDA.publicKey
    );
    expect(electionAccount.stage).to.deep.equal({ voting: {} });
  });
  it("Prevents false voting in voting stage", async () => {
    const [voterAccount] = PublicKey.findProgramAddressSync(
      [
        Buffer.from("voter"),
        electionPDA.publicKey.toBuffer(),
        Buffer.from("voter2"),
      ],
      program.programId
    );

    const [candidateAccount] = PublicKey.findProgramAddressSync(
      [
        Buffer.from("candidate"),
        electionPDA.publicKey.toBuffer(),
        Buffer.from(candidateName),
      ],
      program.programId
    );

    try {
      await program.methods
        .vote("voter2", candidateName)
        .accounts({
          candidateData: candidateAccount,
          electionData: electionPDA.publicKey,
          voterData: voterAccount,
          signer: provider.wallet.publicKey,
        })
        .signers([])
        .rpc();
      expect.fail("Expected an error");
    } catch (error) {
      expect(error.message).to.include("Voter is not whitelisted.");
    }
  });
  it("Casts a vote", async () => {
    const [voterAccount] = PublicKey.findProgramAddressSync(
      [
        Buffer.from("voter"),
        electionPDA.publicKey.toBuffer(),
        Buffer.from(voterId),
      ],
      program.programId
    );

    const [candidateAccount] = PublicKey.findProgramAddressSync(
      [
        Buffer.from("candidate"),
        electionPDA.publicKey.toBuffer(),
        Buffer.from(candidateName),
      ],
      program.programId
    );
    const candidateDataPrev = await program.account.candidateData.fetch(
      candidateAccount
    );
    await program.methods
      .vote(voterId, candidateName)
      .accounts({
        electionData: electionPDA.publicKey,
        candidateData: candidateAccount,
        voterData: voterAccount,
        signer: provider.wallet.publicKey,
      })
      .signers([])
      .rpc();

    const candidateData = await program.account.candidateData.fetch(
      candidateAccount
    );

    const voterData = await program.account.voterData.fetch(voterAccount);
    const electionAccount = await program.account.electionData.fetch(
      electionPDA.publicKey
    );
    expect(electionAccount.stage).to.deep.equal({ voting: {} });
    expect(candidateData.votes.toNumber()).to.equal(
      candidateDataPrev.votes.toNumber() + 1
    );
    expect(voterData.voted).to.be.true;
  });
  it("Prevents double voting", async () => {
    const [voterAccount] = PublicKey.findProgramAddressSync(
      [
        Buffer.from("voter"),
        electionPDA.publicKey.toBuffer(),
        Buffer.from(voterId),
      ],
      program.programId
    );

    const [candidateAccount] = PublicKey.findProgramAddressSync(
      [
        Buffer.from("candidate"),
        electionPDA.publicKey.toBuffer(),
        Buffer.from(candidateName),
      ],
      program.programId
    );

    try {
      await program.methods
        .vote(voterId, candidateName)
        .accounts({
          candidateData: candidateAccount,
          electionData: electionPDA.publicKey,
          voterData: voterAccount,
          signer: provider.wallet.publicKey,
        })
        .signers([])
        .rpc();
      expect.fail("Expected an error");
    } catch (error) {
      expect(error.message).to.include("already in use");
    }
  });
  it("Changes election stage to closed", async () => {
    await program.methods
      .changeStage({ closed: {} })
      .accounts({
        electionData: electionPDA.publicKey,
        initiator: provider.wallet.publicKey,
      })
      .signers([])
      .rpc();

    const electionAccount = await program.account.electionData.fetch(
      electionPDA.publicKey
    );
    expect(electionAccount.stage).to.deep.equal({ closed: {} });
  });



  /// new tests
  
  /*
  
  it("Should prevent voting in application stage", async () => {
    await program.methods
      .addToVoterWhitelist(voterId)
      .accounts({
        electionData: electionPDA.publicKey,
        initiator: provider.wallet.publicKey,
      })
      .rpc();

    const [voterAccount] = PublicKey.findProgramAddressSync(
      [Buffer.from("voter"), electionPDA.publicKey.toBuffer(), Buffer.from(voterId)],
      program.programId
    );

    const [candidateAccount] = PublicKey.findProgramAddressSync(
      [Buffer.from("candidate"), electionPDA.publicKey.toBuffer(), Buffer.from(candidateName)],
      program.programId
    );

    try {
      await program.methods
        .vote(voterId, candidateName)
        .accounts({
          candidateData: candidateAccount,
          electionData: electionPDA.publicKey,
          voterData: voterAccount,
          initiator: provider.wallet.publicKey,
        })
        .signers([])
        .rpc();
      expect.fail("Expected error due to wrong stage");
    } catch (error) {
      //expect(error.message).to.include("InvalidStage");
      console.log(error);
    }
  });

  it("Should prevent registering non-whitelisted candidate", async () => {
    const [candidateAccount] = PublicKey.findProgramAddressSync(
      [Buffer.from("candidate"), electionPDA.publicKey.toBuffer(), Buffer.from(candidateName)],
      program.programId
    );

    try {
      await program.methods
        .registerCandidate(candidateName)
        .accounts({
          candidateData: candidateAccount,
          electionData: electionPDA.publicKey,
          initiator: provider.wallet.publicKey,
        })
        .signers([])
        .rpc();
      expect.fail("Voter is not whitelisted.");
    } catch (error) {
      expect(error.message).to.include("NotWhitelisted");
    }
  });

  it("Should prevent voting for non-registered candidate", async () => {
    // Add voter to whitelist
    await program.methods
      .addToVoterWhitelist(voterId)
      .accounts({
        electionData: electionPDA.publicKey,
        initiator: provider.wallet.publicKey,
      })
      .rpc();

    // Change to voting stage
    await program.methods
      .changeStage({ voting: {} })
      .accounts({
        electionData: electionPDA.publicKey,
        initiator: provider.wallet.publicKey,
      })
      .rpc();

    const [voterAccount] = PublicKey.findProgramAddressSync(
      [Buffer.from("voter"), electionPDA.publicKey.toBuffer(), Buffer.from(voterId)],
      program.programId
    );

    const [candidateAccount] = PublicKey.findProgramAddressSync(
      [Buffer.from("candidate"), electionPDA.publicKey.toBuffer(), Buffer.from("nonexistent")],
      program.programId
    );

    try {
      await program.methods
        .vote(voterId, "nonexistent")
        .accounts({
          candidateData: candidateAccount,
          electionData: electionPDA.publicKey,
          voterData: voterAccount,
          initiator: provider.wallet.publicKey,
        })
        .signers([])
        .rpc();
      expect.fail("Expected error due to non-registered candidate");
    } catch (error) {
      expect(error.message).to.include("Account does not exist");
    }
  });

  */

  // it("Should get election results after voting process", async () => {
  //   // Setup candidate names and voter IDs
  //   const candidate1 = "Alice";
  //   const candidate2 = "Bob";
  //   const voter1 = "voter4";
  //   const voter2 = "voter2";
  //   const voter3 = "voter3";
    
  //     // Set stage to Application
  //   await program.methods
  //   .changeStage({ application: {} })
  //   .accounts({
  //     electionData: electionPDA.publicKey,
  //     initiator: provider.wallet.publicKey,
  //   })
  //   .rpc();

  //   // Add candidates to whitelist
  //   await program.methods
  //     .addToCandidateWhitelist(candidate1)
  //     .accounts({
  //       electionData: electionPDA.publicKey,
  //       initiator: provider.wallet.publicKey,
  //     })
  //     .rpc();
  
  //   await program.methods
  //     .addToCandidateWhitelist(candidate2)
  //     .accounts({
  //       electionData: electionPDA.publicKey,
  //       initiator: provider.wallet.publicKey,
  //     })
  //     .rpc();
  
  //   // Register candidates
  //   const [candidate1Account] = PublicKey.findProgramAddressSync(
  //     [Buffer.from("candidate"), electionPDA.publicKey.toBuffer(), Buffer.from(candidate1)],
  //     program.programId
  //   );
  
  //   const [candidate2Account] = PublicKey.findProgramAddressSync(
  //     [Buffer.from("candidate"), electionPDA.publicKey.toBuffer(), Buffer.from(candidate2)],
  //     program.programId
  //   );
  
  //   await program.methods
  //     .registerCandidate(candidate1)
  //     .accounts({
  //       candidateData: candidate1Account,
  //       electionData: electionPDA.publicKey,
  //       signer: provider.wallet.publicKey,
  //       //systemProgram: SystemProgram.programId,
  //     })
  //     .rpc();
  
  //   await program.methods
  //     .registerCandidate(candidate2)
  //     .accounts({
  //       candidateData: candidate2Account,
  //       electionData: electionPDA.publicKey,
  //       signer: provider.wallet.publicKey,
  //       //systemProgram: SystemProgram.programId,
  //     })
  //     .rpc();
  
  //   // Add voters to whitelist
  //   for (const voterId of [voter1, voter2, voter3]) {
  //     await program.methods
  //       .addToVoterWhitelist(voterId)
  //       .accounts({
  //         electionData: electionPDA.publicKey,
  //         initiator: provider.wallet.publicKey,
  //       })
  //       .rpc();
  //   }
  
  //   // Change stage to voting
  //   await program.methods
  //     .changeStage({ voting: {} })
  //     .accounts({
  //       electionData: electionPDA.publicKey,
  //       initiator: provider.wallet.publicKey,
  //     })
  //     .rpc();
  
  //   // Cast votes
  //   const voteDistribution = [
  //     { voter: voter1, candidate: candidate1 },
  //     { voter: voter2, candidate: candidate1 },
  //     { voter: voter3, candidate: candidate2 },
  //   ];
  
  //   for (const vote of voteDistribution) {
  //     const [voterAccount] = PublicKey.findProgramAddressSync(
  //       [Buffer.from("voter"), electionPDA.publicKey.toBuffer(), Buffer.from(vote.voter)],
  //       program.programId
  //     );
  
  //     const [candidateAccount] = PublicKey.findProgramAddressSync(
  //       [Buffer.from("candidate"), electionPDA.publicKey.toBuffer(), Buffer.from(vote.candidate)],
  //       program.programId
  //     );
  
  //     await program.methods
  //       .vote(vote.voter, vote.candidate)
  //       .accounts({
  //         voterData: voterAccount,
  //         candidateData: candidateAccount,
  //         electionData: electionPDA.publicKey,
  //         signer: provider.wallet.publicKey,
          
  //       })
  //       .rpc();
  //   }
  
  //   // Change stage to closed
  //   await program.methods
  //     .changeStage({ closed: {} })
  //     .accounts({
  //       electionData: electionPDA.publicKey,
  //       initiator: provider.wallet.publicKey,
  //     })
  //     .rpc();
  
  //   // Get and print results
  //   const results = await program.methods
  //     .getElectionResults()
  //     .accounts({
  //       electionData: electionPDA.publicKey,
  //     })
  //     .view();
  
  //   // Add debug logging
  //   console.log("Raw Results:", results);
  //   console.log("Election state:", await program.account.electionData.fetch(electionPDA.publicKey));
  
  //   console.log("Election Results:");
  //   results.forEach(result => {
  //     console.log(`${result.candidateName}: ${result.votes.toString()} votes`);
  //   });
  
  //   // Verify results
  //   expect(results).to.have.lengthOf(2);
  //   const alice = results.find(r => r.candidateName === candidate1);
  //   const bob = results.find(r => r.candidateName === candidate2);
  //   expect(alice.votes.toNumber()).to.equal(2);
  //   expect(bob.votes.toNumber()).to.equal(1);
  // });


  


});
