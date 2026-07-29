// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Complete-language example program:
// A small in-memory order service using ownership, Arc, Mutex, channels and generics.



import thread, sync, stdio;

data Product {
    sku: Str;
    name: Str;
    stock: Int;
}

data OrderLine {
    sku: Str;
    quantity: Int;
}

data Order {
    id: Int;
    lines: ArrayList<OrderLine>;
}

data Receipt {
    order_id: Int;
    accepted: Bool;
    message: Str;
}

interface Repository<K, V> {
    fn get(key: K) -> Result<&V, Error>;
    fn put(key: K, value: V);
}

class InventoryRepository : Repository<Str, Product> {

    products: [Str: Product];

    InventoryRepository() {
        self.products = [];
    }

    fn get(key: Str) -> Result<&Product, Error> {
        if (!self.products.contains(key)) {
            return Error(new Error("unknown product"));
        }
        return Ok(&self.products[key]);
    }

    fn put(key: Str, value: Product) {
        self.products[key] = value;
    }

    fn reserve(line: OrderLine) -> Result<()> {
        product: &mut Product = &mut self.products[line.sku];

        if (product.stock < line.quantity) {
            return Error(new Error("not enough stock"));
        }

        product.stock -= line.quantity;
        return Ok();
    }
}

class OrderWorker {
    inventory: Arc<Mutex<InventoryRepository>>;

    OrderWorker(inventory: Arc<Mutex<InventoryRepository>>) {
        self.inventory = inventory;
    }

    fn process(order: Order) -> Receipt {
        guard: Mutex<InventoryRepository> = self.inventory.lock();

        for (line: OrderLine in order.lines) {
            result: Result<()> = (*guard).reserve(line);
            match (result) {
                Ok(else) -> {},
                Error(error) -> {
                    return Receipt {
                        order_id: order.id,
                        accepted: false,
                        message: error.to_string(),
                    };
                },
            }
        }

        return Receipt {
            order_id: order.id,
            accepted: true,
            message: "accepted",
        };
    }
}

fn seed_inventory() -> Arc<Mutex<InventoryRepository>> {
    repository: InventoryRepository = new InventoryRepository();

    repository.put("book", Product {
        sku: "book",
        name: "X# Handbook",
        stock: 10,
    });

    repository.put("mug", Product {
        sku: "mug",
        name: "Compiler Mug",
        stock: 5,
    });

    return new Arc<Mutex<InventoryRepository>>(new Mutex<InventoryRepository>(repository));
}

fn make_order(id: Int, sku: Str, quantity: Int) -> Order {
    lines: ArrayList<OrderLine> = [];
    lines.append(OrderLine {
        sku: sku,
        quantity: quantity,
    });

    return Order {
        id: id,
        lines: lines,
    };
}

fn main() -> Result<()> {
    inventory: Arc<Mutex<InventoryRepository>> = seed_inventory();

    (orders, receipts): std::thread::Channel<Order> = std::thread::channel::<Order>();
    (results, result_reader): std::thread::Channel<Receipt> = std::thread::channel::<Receipt>();

    worker_inventory: Arc<Mutex<InventoryRepository>> = Arc::clone(&inventory);

    std::thread::spawn(move fn() {
        worker: OrderWorker = new OrderWorker(worker_inventory);

        while (true) {
            maybe_order: Result<Order, Error> = receipts.recv();
            if (maybe_order.is_error()) {
                break;
            }
            receipt: Receipt = worker.process(maybe_order.unwrap());
            results.send(receipt)@;
        }
    });

    orders.send(make_order(1, "book", 2))@;
    orders.send(make_order(2, "mug", 8))@;
    orders.close();

    while (true) {
        maybe_receipt: Result<Receipt, Error> = result_reader.recv();
        if (maybe_receipt.is_error()) {
            break;
        }
        receipt: Receipt = maybe_receipt.unwrap();
        println!(
            "order #{} accepted={} message={}",
            receipt.order_id,
            receipt.accepted,
            receipt.message
        );
    }

    return Ok();
}
