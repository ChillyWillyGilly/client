﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace CitizenFX.Core
{
    public static class ScriptEnvironment
    {
        public static string ResourceName
        {
            get
            {
                return RuntimeManager.ResourceName;
            }
        }
    }
}
