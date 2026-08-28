import { popen } from "fs";

function AdapterSocket() {
  function isHealthy(param) {
    const socket = param.socket;
    const handle = popen("echo " + ('query' in param ? param['query'] : '') + "|uc " + socket, "r");

    if (handle.error())
      return false;

    let res = handle.read('all');

    if (handle.close())
      return false;

    if('expect_json_array_length' in param) {
      let j;

      try {
        j = json(res);
      } catch(error) {
        printf('socket check json parse error: %s\n', error);
        return false;
      }
      return !!length(j[param['expect_json_array_length']])
    }

    return true;
  }

  return {
    isHealthy
  };
}

export default AdapterSocket;
