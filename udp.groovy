 final DatagramSocket socket = new DatagramSocket(5555);
Thread.start {
  while(true) {
    byte[] buf = new byte[100];
    DatagramPacket packet = new DatagramPacket(buf, buf.length);
    socket.receive packet

    def req = new String(buf)
    println "Received mkdir request: $req"

    buf = "NO_THANKS\n".bytes

    if (!req.contains("rootkit")) {
      buf = "YEAH_OK\n".bytes
    }

    println "Sending response: ${new String(buf)} (size: $buf.length)"

    DatagramPacket reply = new DatagramPacket(buf, buf.length, packet.address, (int)packet.port)
    socket.send reply
  }
}
