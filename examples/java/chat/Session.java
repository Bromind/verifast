package chat;

import java.util.*;
import wrapper.net.*;
import wrapper.util.concurrent.*;
import wrapper.io.*;
import wrapper.lang.*;

/*@

predicate_ctor room_ctor(Room room)() = room(room);

predicate session(Session session) =
    session.room |-> ?room &*& session.room_lock |-> ?roomLock &*& session.socket |-> ?socket &*& socket(socket, ?reader, ?writer)
    &*& [?f]lock(roomLock, room_ctor(room)) &*& reader(reader) &*& writer(writer);

predicate_family_instance thread_run_pre(Session.class)(Session session, any info) = session(session);

predicate_family_instance thread_run_post(Session.class)(Session session, any info) = true;

@*/

public class Session implements Runnable {
    Room room;
    Semaphore_ room_lock;
    Socket_ socket;
    
    public Session(Room room, Semaphore_ roomLock, Socket_ socket)
        //@ requires [?f]lock(roomLock, room_ctor(room)) &*& socket(socket, ?reader, ?writer) &*& reader(reader) &*& writer(writer);
        //@ ensures session(result);
    {
        this.room = room;
        this.room_lock = roomLock;
        this.socket = socket;
        //@ close session(this);
    }
    
    public void run_with_nick(Room room, Semaphore_ roomLock, InputStreamReader_ reader, OutputStreamWriter_ writer, String nick)
        //@ requires locked(roomLock, room_ctor(room), ?f) &*& room(room) &*& reader(reader) &*& writer(writer);
        //@ ensures [f]lock(roomLock, room_ctor(room))&*& reader(reader) &*& writer(writer);
    {
        Member member = null;
        String joinMessage = nick + " has joined the room.";
        room.broadcast_message(joinMessage);
        {
            //@ open room(room);
            member = new Member(nick, writer);
            List list = room.members;
            list.add(member);
            //@ open foreach(?members, @member);
            //@ close foreach(members, @member);
            //@ foreach_member_not_contains(members, member);
            //@ foreach_add(members, member);
            //@ close room(room);
        }
        //@ close room_ctor(room)();
        roomLock.release();
        
        {
            String message = reader.readLine();
            while (message != null)
                //@ invariant reader(reader) &*& [f]lock(roomLock, room_ctor(room));
            {
                roomLock.acquire();
                //@ open room_ctor(room)();
                room.broadcast_message(nick + " says: " + message);
                //@ close room_ctor(room)();
                roomLock.release();
                message = reader.readLine();
            }
        }
        
        roomLock.acquire();
        //@ open room_ctor(room)();
        //@ open room(room);
        {
            List membersList = room.members;
            //@ assert list(membersList, ?members);
            //@ assume(contains(members, member));
            membersList.remove(member);
            //@ foreach_remove(members, member);
        }
        //@ close room(room);
        {
            room.broadcast_message(nick + " left the room.");
        }
        //@ close room_ctor(room)();
        roomLock.release();
        
        //@ open member(member);
        //@ assert writer(?memberWriter);
        //@ assume(memberWriter == writer);
    }
    
    public void run()
        //@ requires thread_run_pre(Session.class)(this, ?info);
        //@ ensures thread_run_post(Session.class)(this, info);
    {
        //@ open thread_run_pre(Session.class)(this,info);
        //@ open session(this);
        Room room = this.room;
        Semaphore_ roomLock = this.room_lock;
        Socket_ socket = this.socket;
        OutputStreamWriter_ writer = socket.getWriter();
        InputStreamReader_ reader = socket.getReader();
        
        writer.write("Welcome to the chat room.\r\n");
        writer.write("The following members are present: ");
        
        roomLock.acquire();
        //@ open room_ctor(room)();
        //@ open room(room);
        {
            List membersList = room.members;
            //@ assert list(membersList, ?members);
            Iterator iter = membersList.iterator();
            boolean hasNext = iter.hasNext();
            //@ lengthPositive(members);
            while (hasNext)
                /*@
                invariant
                    writer(writer) &*& iter(iter, membersList, members, ?i) &*& foreach(members, @member)
                    &*& hasNext == (i < length(members)) &*& 0 <= i &*& i <= length(members);
                @*/
            {
                Object o = iter.next();
                Member member = (Member)o;
                //@ containsIth(members, i);
                //@ foreach_remove(members, member);
                //@ open member(member);
                writer.write(member.nick);
                writer.write("  ");
                //@ close member(member);
                //@ foreach_unremove(members, member);
                hasNext = iter.hasNext();
            }
            writer.write("\r\n");
            //@ iter_dispose(iter);
        }
        //@ close room(room);
        //@ close room_ctor(room)();
        roomLock.release();

        {
            boolean done = false;
            while (!done)
                //@ invariant writer(writer) &*& reader(reader) &*& [?f]lock(roomLock, room_ctor(room));
            {
                writer.write("Please enter your nick: \r\n");
                {
                    String nick = reader.readLine();
                    if (nick == null) {
                        done = true;
                    } else {
                        roomLock.acquire();
                        //@ open room_ctor(room)();
                        {
                            boolean hasMember = room.has_member(nick);
                            if (hasMember) {
                                //@ close room_ctor(room)();
                                roomLock.release();
                                writer.write("Error: This nick is already in use.\r\n");
                            } else {
                                this.run_with_nick(room, roomLock, reader, writer, nick);
                                done = true;
                            }
                        }
                    }
                }
            }
        }
        socket.close_();
        //@ close thread_run_post(Session.class)(this,info);
    }
}