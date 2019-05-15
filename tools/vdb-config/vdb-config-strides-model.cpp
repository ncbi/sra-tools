struct vdbconf_strides_modelImpl {
    KConfig * config;
};

vdbconf_strides_model::vdbconf_strides_model(KConfig * config){
    allocate vdbconf_strides_modelImpl;
    throw if no memory or config == NULL;
    m_Impl->config = config;
}

all vdbconf_strides_model::does|set|get functions call functions from ncbi-vdb/libs/properties.c
