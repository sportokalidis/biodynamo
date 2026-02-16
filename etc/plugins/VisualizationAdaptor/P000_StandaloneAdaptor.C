void P000_StandaloneAdaptor()
{
  gPluginMgr->AddHandler("VisualizationAdaptor", "standalone",
                        "bdm::StandaloneAdaptor", "VisualizationAdaptor",
                        "Factory()");
// Disabled: Standalone adaptor handler is registered programmatically
// in VisualizationAdaptor::Create(). Keeping this file empty avoids
// ROOT/cling macro parsing errors on some environments.
