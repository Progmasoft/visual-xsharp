// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Complete-language example program:
// A small two-sided ledger with nominal money and audit records.



import stdio, process;

data Money {
    cents: Int;
    currency: String;
}

data Account {
    id: String;
    owner: String;
    balance: Money;
}

data Transfer {
    source_account: String;
    target_account: String;
    amount: Money;
    memo: Optional<String>;
}

class Ledger {
    accounts: [String: Account];
    audit: ArrayList<Transfer>;

    Ledger() {
        self.accounts = [];
        self.audit = [];
    }

    fn open(id: String, owner: String, balance: Money) {
        self.accounts[id] = Account {
            id: id,
            owner: owner,
            balance: balance,
        };
    }

    fn apply(transfer: Transfer) -> Result<()> {
        if (transfer.amount.cents <= 0) {
            return Error(new Error("invalid transfer amount"));
        }

        from_account: &mut Account = self.account_mut(&transfer.source_account)@;
        to_account: &mut Account = self.account_mut(&transfer.target_account)@;

        if (from_account.balance.cents < transfer.amount.cents) {
            return Error(new Error("insufficient funds"));
        }

        from_account.balance.cents -= transfer.amount.cents;
        to_account.balance.cents += transfer.amount.cents;
        self.audit.append(transfer);
        return Ok();
    }

    fn account_mut(id: &Str) -> Result<&mut Account, Error> {
        if (!self.accounts.contains(id)) {
            return Error(new Error("unknown account"));
        }
        return Ok(&mut self.accounts[id]);
    }

    fn print() -> Result<()> {
        for ((id, account): (String, Account) in self.accounts) {
            println!("{} {} {}", id, account.owner, account.balance.cents);
        }
        return Ok();
    }
}

fn main() -> Result<Int, Error> {
    ledger := new Ledger();

    ledger.open(new String("checking"), new String("Ada"), Money {
        cents: 50'000,
        currency: new String("USD"),
    });
    ledger.open(new String("savings"), new String("Ada"), Money {
        cents: 0,
        currency: new String("USD"),
    });

    ledger.apply(Transfer {
        source_account: new String("checking"),
        target_account: new String("savings"),
        amount: Money {
            cents: 12'500,
            currency: new String("USD"),
        },
        memo: Some(new String("monthly savings")),
    })@;

    ledger.print()@;
    return Ok(0);
}
