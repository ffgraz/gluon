let ubus = require("ubus");
let uloop = require("uloop");
let conn = ubus.connect();

// We always assume one instance because fuck it
function AdapterProcd() {
  const onService = {};

  function isHealthy(name) {
    assert(name);

    const res = conn.call('service', 'list', { name });
    // FIXME: service is not currently there for reasons
    if (!res[name]) return false;
    const instances = res[name].instances;
    const instanceList = keys(instances);
    if (!length(instanceList)) return false;

    const first = instances[instanceList[0]];

    return first.running;
  }

  let sub = conn.subscriber(
    function(notify) {
      const data = notify.data;
      const service = data.service;
      if (!service) return;

      const cb = onService[service];

      if (cb) {
        uloop.timer(100, () => {
          cb(isHealthy(service));
        });
      }

      // "ubus subscribe service" gives us the event type,
      // but I can't figure out how to get it
      // so this is now the above hack instead

      /* if ('service.stop' in data) {
        const service = data['service.stop'].service;
        if (onService[service]) {
          onService[service](false);
        }
      }

      if ('service.start' in data) {
        const service = data['service.start'].service;
        if (onService[service]) {
          onService[service](true);
        }
      } */
    },
    function(id) {
      printf("Object %x gone away\n", id);
    }
  );

  if (!sub.subscribe("service"))
    warn("Subscribe error: " + ubus.error());

  return {
    isHealthy,
    subscribe: (name, onChange) => {
      onService[name] = onChange;
    }
  }
}

export default AdapterProcd;
