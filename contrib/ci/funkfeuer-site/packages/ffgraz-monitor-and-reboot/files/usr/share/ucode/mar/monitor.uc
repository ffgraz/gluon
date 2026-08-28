function timePassed (base, howMuch) {
  return base <= time() - howMuch;
}

const MAX_GRACE = 60 * 60;

function ServiceMonitor(name, config, isHealthy, storage, poll, IS_PROD) {
  let state = isHealthy ? 'healthy' : 'dead';
  let deadSince = isHealthy ? null : storage.get(name, MAX_GRACE)[0] || time();
  let revives = isHealthy ? 0 : length(storage.get(name, MAX_GRACE));
  let maxRevives = config.attempts_until_reboot ?? 5;

  const tryReviveAfter = 'downtime_until_restart' in config
    ? config.downtime_until_restart : 10;

  function tryRecover() {
    const commands = config.restart ?? [`service ${name} restart`];
    printf('Fixing %J: %s/%s\n', name, revives, maxRevives);

    for (let i = 0; i < length(commands); i++) {
      printf(' + %s\n', commands[i]);
      system(commands[i], 10 * 1000);
    }
  }

  function machine(newIsHealthy) {
    // we're getting new healthy state
    if (newIsHealthy === true || newIsHealthy === false) {
      isHealthy = newIsHealthy;
    }

    // printf('State machine %s %s %J\n', name, state, isHealthy);

    switch (state) {
      case 'healthy': {
        if (!isHealthy) {
          state = 'dead';
          deadSince = time();
          printf('%s is no longer healthy\n', name);
        }
        break;
      }
      case 'dead': {
        if (isHealthy) {
          // we're no longer dead
          printf('%s is healthy again\n', name);
          state = 'healthy';
          revives = 0;
          deadSince = null;
          storage.clear(name);
          break;
        }
        // will get rid of all attempts older than an hour
        revives = length(storage.get(name, MAX_GRACE));
        if (revives >= maxRevives) {
          if (!config.disable_reboot) {
            // service is unfixable, prepare for reboot
            const reboots = storage.get('REBOOT', 24 * 60 * 60);
            if (length(reboots) < 3) {
              storage.append('REBOOT', 24 * 60 * 60);
              printf('Rebooting...\n');
              system('sleep 3s');
              if(IS_PROD)
                system('reboot');
              else
                exit(1);
            }
            break;
          }

          printf('%s is currently broken and could not be repaired\n', name);
          break;
        }
        if (timePassed(deadSince, tryReviveAfter)) {
          // poll again to be really sure
          isHealthy = poll();
          if (isHealthy) {
            machine();
            break;
          }

          revives++;
          storage.append(name, MAX_GRACE);
          tryRecover();
          // queue another revieve if necesarry
          deadSince = time();
          // wait a bit
          system('sleep 2s');
          // check if we're back
          machine(poll());
          break;
        }
      }
    }
  }

  return {
    machine
  }
}

export default ServiceMonitor;
