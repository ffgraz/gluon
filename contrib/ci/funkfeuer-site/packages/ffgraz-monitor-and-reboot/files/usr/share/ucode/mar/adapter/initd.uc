function AdapterInitd() {
  function isHealthy(name) {
    // 0=success
    return !system(`service ${name} running`)
  }

  return {
    isHealthy
  };
}

export default AdapterInitd;
