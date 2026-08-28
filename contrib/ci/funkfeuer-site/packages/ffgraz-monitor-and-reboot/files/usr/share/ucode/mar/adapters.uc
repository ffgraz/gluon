import initd from 'mar.adapter.initd';
import procd from 'mar.adapter.procd';
import socket from 'mar.adapter.socket';

export default {
  initd: initd(),
  procd: procd(),
  socket: socket(),
};
