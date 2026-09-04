# VM images for gateway.py: one gateway whose mesh daemon matches the
# image under test. Import <gluon/gateway/batman-adv>, .../babel or
# .../olsrd instead to pin one.
{
  gateway = {
    imports = [ <gluon/gateway> ];
  };
}
