    #ifndef _HI_MRIM_PROTOCOL_H
    #define _HI_MRIM_PROTOCOL_H
     
    #include "abstractprotocol.h"
     
    class MRIMProtocol : AbstractProtocol
    {
            public:
                    MRIMProtocol();
                    ~MRIMProtocol();
                    const std::string gatewayIdentity() { return "mrim"; }
    //              const std::string protocol() { return "prpl-ostin-mrim"; }
                    const std::string protocol() { return "prpl-mra"; }
                    Tag *getVCardTag(AbstractUser *user, GList *vcardEntries);
                    std::list<std::string> transportFeatures();
                    std::list<std::string> buddyFeatures();
                    std::string text(const std::string &key);
            private:
                    std::list<std::string> m_transportFeatures;
                    std::list<std::string> m_buddyFeatures;
     
    };
     
    #endif

