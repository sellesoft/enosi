enum CLIArgType
{
 Unknown,
 VerboseBig,
 OptionTrace,
 OptionDebug,
 OptionInfo,
 OptionNotice,
 OptionWarn,
 OptionError,
 OptionFatal,
 VerboseSmall,
 JobsSmall,
 JobsBig,
 PrintTransformed,
};
static CLIArgType parse_cli_arg(const char* arg)
{
 switch (*arg)
 {
  case 'i':
   arg += 1;
   switch (*arg)
   {
    case 'n':
     arg += 1;
     switch (*arg)
     {
      case 'f':
       arg += 1;
       switch (*arg)
       {
        case 'o':
         arg += 1;
         switch (*arg)
         {
          default:
           return CLIArgType::Unknown;
           break;
          case '\0':
           return CLIArgType::OptionInfo;
           break;
         }
         break;
       }
       break;
     }
     break;
   }
   break;
  case 'd':
   arg += 1;
   switch (*arg)
   {
    case 'e':
     arg += 1;
     switch (*arg)
     {
      case 'b':
       arg += 1;
       switch (*arg)
       {
        case 'u':
         arg += 1;
         switch (*arg)
         {
          case 'g':
           arg += 1;
           switch (*arg)
           {
            default:
             return CLIArgType::Unknown;
             break;
            case '\0':
             return CLIArgType::OptionDebug;
             break;
           }
           break;
         }
         break;
       }
       break;
     }
     break;
   }
   break;
  case '-':
   arg += 1;
   switch (*arg)
   {
    case 'j':
     arg += 1;
     switch (*arg)
     {
      default:
       return CLIArgType::Unknown;
       break;
      case '\0':
       return CLIArgType::JobsSmall;
       break;
     }
     break;
    case '-':
     arg += 1;
     switch (*arg)
     {
      case 'm':
       arg += 1;
       switch (*arg)
       {
        case 'a':
         arg += 1;
         switch (*arg)
         {
          case 'x':
           arg += 1;
           switch (*arg)
           {
            case '-':
             arg += 1;
             switch (*arg)
             {
              case 'j':
               arg += 1;
               switch (*arg)
               {
                case 'o':
                 arg += 1;
                 switch (*arg)
                 {
                  case 'b':
                   arg += 1;
                   switch (*arg)
                   {
                    case 's':
                     arg += 1;
                     switch (*arg)
                     {
                      default:
                       return CLIArgType::Unknown;
                       break;
                      case '\0':
                       return CLIArgType::JobsBig;
                       break;
                     }
                     break;
                   }
                   break;
                 }
                 break;
               }
               break;
             }
             break;
           }
           break;
         }
         break;
       }
       break;
      case 'p':
       arg += 1;
       switch (*arg)
       {
        case 'r':
         arg += 1;
         switch (*arg)
         {
          case 'i':
           arg += 1;
           switch (*arg)
           {
            case 'n':
             arg += 1;
             switch (*arg)
             {
              case 't':
               arg += 1;
               switch (*arg)
               {
                case '-':
                 arg += 1;
                 switch (*arg)
                 {
                  case 't':
                   arg += 1;
                   switch (*arg)
                   {
                    case 'r':
                     arg += 1;
                     switch (*arg)
                     {
                      case 'a':
                       arg += 1;
                       switch (*arg)
                       {
                        case 'n':
                         arg += 1;
                         switch (*arg)
                         {
                          case 's':
                           arg += 1;
                           switch (*arg)
                           {
                            case 'f':
                             arg += 1;
                             switch (*arg)
                             {
                              case 'o':
                               arg += 1;
                               switch (*arg)
                               {
                                case 'r':
                                 arg += 1;
                                 switch (*arg)
                                 {
                                  case 'm':
                                   arg += 1;
                                   switch (*arg)
                                   {
                                    case 'e':
                                     arg += 1;
                                     switch (*arg)
                                     {
                                      case 'd':
                                       arg += 1;
                                       switch (*arg)
                                       {
                                        default:
                                         return CLIArgType::Unknown;
                                         break;
                                        case '\0':
                                         return CLIArgType::PrintTransformed;
                                         break;
                                       }
                                       break;
                                     }
                                     break;
                                   }
                                   break;
                                 }
                                 break;
                               }
                               break;
                             }
                             break;
                           }
                           break;
                         }
                         break;
                       }
                       break;
                     }
                     break;
                   }
                   break;
                 }
                 break;
               }
               break;
             }
             break;
           }
           break;
         }
         break;
       }
       break;
      case 'v':
       arg += 1;
       switch (*arg)
       {
        case 'e':
         arg += 1;
         switch (*arg)
         {
          case 'r':
           arg += 1;
           switch (*arg)
           {
            case 'b':
             arg += 1;
             switch (*arg)
             {
              case 'o':
               arg += 1;
               switch (*arg)
               {
                case 's':
                 arg += 1;
                 switch (*arg)
                 {
                  case 'i':
                   arg += 1;
                   switch (*arg)
                   {
                    case 't':
                     arg += 1;
                     switch (*arg)
                     {
                      case 'y':
                       arg += 1;
                       switch (*arg)
                       {
                        default:
                         return CLIArgType::Unknown;
                         break;
                        case '\0':
                         return CLIArgType::VerboseBig;
                         break;
                       }
                       break;
                     }
                     break;
                   }
                   break;
                 }
                 break;
               }
               break;
             }
             break;
           }
           break;
         }
         break;
       }
       break;
     }
     break;
    case 'v':
     arg += 1;
     switch (*arg)
     {
      default:
       return CLIArgType::Unknown;
       break;
      case '\0':
       return CLIArgType::VerboseSmall;
       break;
     }
     break;
   }
   break;
  case 'e':
   arg += 1;
   switch (*arg)
   {
    case 'r':
     arg += 1;
     switch (*arg)
     {
      case 'r':
       arg += 1;
       switch (*arg)
       {
        case 'o':
         arg += 1;
         switch (*arg)
         {
          case 'r':
           arg += 1;
           switch (*arg)
           {
            default:
             return CLIArgType::Unknown;
             break;
            case '\0':
             return CLIArgType::OptionError;
             break;
           }
           break;
         }
         break;
       }
       break;
     }
     break;
   }
   break;
  case 'n':
   arg += 1;
   switch (*arg)
   {
    case 'o':
     arg += 1;
     switch (*arg)
     {
      case 't':
       arg += 1;
       switch (*arg)
       {
        case 'i':
         arg += 1;
         switch (*arg)
         {
          case 'c':
           arg += 1;
           switch (*arg)
           {
            case 'e':
             arg += 1;
             switch (*arg)
             {
              default:
               return CLIArgType::Unknown;
               break;
              case '\0':
               return CLIArgType::OptionNotice;
               break;
             }
             break;
           }
           break;
         }
         break;
       }
       break;
     }
     break;
   }
   break;
  case 'f':
   arg += 1;
   switch (*arg)
   {
    case 'a':
     arg += 1;
     switch (*arg)
     {
      case 't':
       arg += 1;
       switch (*arg)
       {
        case 'a':
         arg += 1;
         switch (*arg)
         {
          case 'l':
           arg += 1;
           switch (*arg)
           {
            default:
             return CLIArgType::Unknown;
             break;
            case '\0':
             return CLIArgType::OptionFatal;
             break;
           }
           break;
         }
         break;
       }
       break;
     }
     break;
   }
   break;
  case 't':
   arg += 1;
   switch (*arg)
   {
    case 'r':
     arg += 1;
     switch (*arg)
     {
      case 'a':
       arg += 1;
       switch (*arg)
       {
        case 'c':
         arg += 1;
         switch (*arg)
         {
          case 'e':
           arg += 1;
           switch (*arg)
           {
            default:
             return CLIArgType::Unknown;
             break;
            case '\0':
             return CLIArgType::OptionTrace;
             break;
           }
           break;
         }
         break;
       }
       break;
     }
     break;
   }
   break;
  case 'w':
   arg += 1;
   switch (*arg)
   {
    case 'a':
     arg += 1;
     switch (*arg)
     {
      case 'r':
       arg += 1;
       switch (*arg)
       {
        case 'n':
         arg += 1;
         switch (*arg)
         {
          default:
           return CLIArgType::Unknown;
           break;
          case '\0':
           return CLIArgType::OptionWarn;
           break;
         }
         break;
       }
       break;
     }
     break;
   }
   break;
 }
 return CLIArgType::Unknown;
}
