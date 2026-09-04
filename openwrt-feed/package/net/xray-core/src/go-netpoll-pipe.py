"""Replace the Go netpoll wakeup eventfd with a pipe.

Go 1.21 and later use an eventfd for netpollBreak.  On the HH71VM's Realtek 4.14
kernel, writing to an eventfd that is registered in an epoll set corrupts kernel
memory and panics the board (a 30-line C reproducer says so; a pipe and a
socketpair in the same shape are both fine).  Before Go 1.21 the runtime used a
pipe here, so this puts that back.
"""
import io, re, sys

path = sys.argv[1] + "/src/runtime/netpoll_epoll.go"
s = io.open(path, encoding="utf-8", newline="").read()
if "netpollBreakRd" in s:
    print("already patched")
    sys.exit(0)

s = s.replace(
    """var (
	epfd           int32         = -1 // epoll descriptor
	netpollEventFd uintptr            // eventfd for netpollBreak
	netpollWakeSig atomic.Uint32      // used to avoid duplicate calls of netpollBreak
)""",
    """var (
	epfd           int32         = -1 // epoll descriptor
	netpollBreakRd uintptr            // for netpollBreak
	netpollBreakWr uintptr            // for netpollBreak
	netpollWakeSig atomic.Uint32      // used to avoid duplicate calls of netpollBreak
)""")

s = s.replace(
    """	efd, errno := linux.Eventfd(0, linux.EFD_CLOEXEC|linux.EFD_NONBLOCK)
	if errno != 0 {
		println("runtime: eventfd failed with", errno)
		throw("runtime: eventfd failed")
	}
	ev := linux.EpollEvent{
		Events: linux.EPOLLIN,
	}
	*(**uintptr)(unsafe.Pointer(&ev.Data)) = &netpollEventFd
	errno = linux.EpollCtl(epfd, linux.EPOLL_CTL_ADD, efd, &ev)
	if errno != 0 {
		println("runtime: epollctl failed with", errno)
		throw("runtime: epollctl failed")
	}
	netpollEventFd = uintptr(efd)
}""",
    """	r, w, errpipe := nonblockingPipe()
	if errpipe != 0 {
		println("runtime: pipe failed with", -errpipe)
		throw("runtime: pipe failed")
	}
	ev := linux.EpollEvent{
		Events: linux.EPOLLIN,
	}
	*(**uintptr)(unsafe.Pointer(&ev.Data)) = &netpollBreakRd
	errno = linux.EpollCtl(epfd, linux.EPOLL_CTL_ADD, r, &ev)
	if errno != 0 {
		println("runtime: epollctl failed with", errno)
		throw("runtime: epollctl failed")
	}
	netpollBreakRd = uintptr(r)
	netpollBreakWr = uintptr(w)
}""")

s = s.replace(
    """func netpollIsPollDescriptor(fd uintptr) bool {
	return fd == uintptr(epfd) || fd == netpollEventFd
}""",
    """func netpollIsPollDescriptor(fd uintptr) bool {
	return fd == uintptr(epfd) || fd == netpollBreakRd || fd == netpollBreakWr
}""")

s = s.replace(
    """	var one uint64 = 1
	oneSize := int32(unsafe.Sizeof(one))
	for {
		n := write(netpollEventFd, noescape(unsafe.Pointer(&one)), oneSize)
		if n == oneSize {
			break
		}""",
    """	for {
		var b byte
		n := write(netpollBreakWr, noescape(unsafe.Pointer(&b)), 1)
		if n == 1 {
			break
		}""")

s = s.replace(
    """		if *(**uintptr)(unsafe.Pointer(&ev.Data)) == &netpollEventFd {
			if ev.Events != linux.EPOLLIN {
				println("runtime: netpoll: eventfd ready for", ev.Events)
				throw("runtime: netpoll: eventfd ready for something unexpected")
			}
			if delay != 0 {
				// netpollBreak could be picked up by a
				// nonblocking poll. Only read the 8-byte
				// integer if blocking.
				// Since EFD_SEMAPHORE was not specified,
				// the eventfd counter will be reset to 0.
				var one uint64
				read(int32(netpollEventFd), noescape(unsafe.Pointer(&one)), int32(unsafe.Sizeof(one)))
				netpollWakeSig.Store(0)
			}
			continue
		}""",
    """		if *(**uintptr)(unsafe.Pointer(&ev.Data)) == &netpollBreakRd {
			if ev.Events != linux.EPOLLIN {
				println("runtime: netpoll: break fd ready for", ev.Events)
				throw("runtime: netpoll: break fd ready for something unexpected")
			}
			if delay != 0 {
				// netpollBreak could be picked up by a
				// nonblocking poll. Only read the byte
				// if blocking.
				var tmp [16]byte
				read(int32(netpollBreakRd), noescape(unsafe.Pointer(&tmp[0])), int32(len(tmp)))
				netpollWakeSig.Store(0)
			}
			continue
		}""")

if "netpollEventFd" in s:
    print("FAILED: netpollEventFd still referenced")
    for i, line in enumerate(s.split("\n"), 1):
        if "netpollEventFd" in line:
            print(f"  {i}: {line}")
    sys.exit(1)
io.open(path, "w", encoding="utf-8", newline="").write(s)
print("patched", path)
