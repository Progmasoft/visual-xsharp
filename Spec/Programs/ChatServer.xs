// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Complete-language example program:
// A tiny multi-room chat server using async sockets and channels.



import sync, thread, net;

data ClientId {
    value: Int;
}

data Message {
    room: String;
    sender: ClientId;
    body: String;
}

class Room {
    name: String;
    members: [ClientId: std::thread::Sender<Message>];

    Room(name: String) {
        self.name = name;
        self.members = [];
    }

    fn join(id: ClientId, sender: std::thread::Sender<Message>) {
        self.members[id] = sender;
    }

    fn leave(id: ClientId) {
        self.members.remove(id);
    }

    fn broadcast(message: Message) {
        for ((else, sender): (ClientId, std::thread::Sender<Message>) in self.members) {
            sender.send(message);
        }
    }
}

class ChatHub {
    next_id: std::sync::Atomic<Int>;
    rooms: std::sync::Arc<std::sync::Mutex<[String: Room]>>;

    ChatHub() {
        self.next_id = new std::sync::Atomic<Int>(1);
        self.rooms = new std::sync::Arc<std::sync::Mutex<[String: Room]>>(new std::sync::Mutex<[String: Room]>([]));
    }

    async fn serve(listener: std::net::TcpListener) -> Task<Result<()>> {
        while (true) {
            socket: std::net::TcpStream = await listener.accept()@;
            id: ClientId = ClientId {
                value: self.next_id.fetch_add(1),
            };
            rooms: std::sync::Arc<std::sync::Mutex<[String: Room]>> = std::sync::Arc::clone(&self.rooms);

            std::thread::spawn(move async fn() {
                session: ClientSession = new ClientSession(id, socket, rooms);
                await session.run()@;
            });
        }
    }
}

class ClientSession {
    id: ClientId;
    socket: std::net::TcpStream;
    rooms: std::sync::Arc<std::sync::Mutex<[String: Room]>>;

    ClientSession(id: ClientId, socket: std::net::TcpStream, rooms: std::sync::Arc<std::sync::Mutex<[String: Room]>>) {
        self.id = id;
        self.socket = socket;
        self.rooms = rooms;
    }

    async fn run() -> Task<Result<()>> {
        current_room: String = new String("lobby");
        outbound: std::thread::Channel<Message> = std::thread::channel::<Message>();
        self.join(&current_room, outbound.sender());

        while (true) {
            line: Optional<String> = await self.socket.read_line();
            command: String = line ?? new String("/quit");

            if (command == "/quit") {
                self.leave(&current_room);
                return Ok();
            }

            if (command.starts_with("/join ")) {
                self.leave(&current_room);
                current_room = command.substring(6);
                self.join(&current_room, outbound.sender());
                continue;
            }

            self.broadcast(Message {
                room: new String(&current_room),
                sender: self.id,
                body: command,
            });
        }
    }

    fn join(room_name: &Str, sender: std::thread::Sender<Message>) {
        guard: std::sync::Mutex<[String: Room]> = self.rooms.lock();
        if (!(*guard).contains(room_name)) {
            (*guard)[new String(room_name)] = new Room(new String(room_name));
        }
        (*guard)[room_name].join(self.id, sender);
    }

    fn leave(room_name: &Str) {
        guard: std::sync::Mutex<[String: Room]> = self.rooms.lock();
        (*guard)[room_name].leave(self.id);
    }

    fn broadcast(message: Message) {
        guard: std::sync::Mutex<[String: Room]> = self.rooms.lock();
        (*guard)[message.room].broadcast(message);
    }
}

async fn main() -> Task<Result<Int, Error>> {
    listener: std::net::TcpListener = await std::net::listen("127.0.0.1:9000")@;
    hub: ChatHub = new ChatHub();
    await hub.serve(listener)@;
    return Ok(0);
}
