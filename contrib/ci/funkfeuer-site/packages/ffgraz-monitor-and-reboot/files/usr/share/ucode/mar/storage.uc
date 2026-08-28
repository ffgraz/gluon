import { writefile, readfile, access, unlink } from "fs";

function s(key) {
  return `/etc/.mar.${key}`
}

function Storage() {
  function get(key, maxAge) {
    const p = s(key);
    if (!access(p, 'f'))
      return [];

    let list = split(readfile(p), '\n');
    list = filter(list, str => !!str);
    list = map(list, p => int(p));

    if (maxAge) {
      const oldest = time() - maxAge;
      list = filter(list, (ts) => ts > oldest);
    }

    return list;
  }

  return {
    get,
    append: (key, maxAge) => {
      const list = get(key, maxAge);
      push(list, time());
      writefile(s(key), join('\n', list));
    },
    clear: (key) => {
      const p = s(key);
      if (access(p, 'f'))
        unlink(p);
    }
  };
}

export default Storage;
